/*
 * sound/soc/codec/aud3003x.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/samsung/abox.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/completion.h>
#include <soc/samsung/acpm_mfd.h>

#include "aud3003x.h"

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err
#endif

void aud3003x_usleep(unsigned int u_sec)
{
	usleep_range(u_sec, u_sec + 10);
}

int aud3003x_acpm_read_reg(unsigned int slave, unsigned int reg,
		unsigned int *val)
{
	int ret;
	u8 reg_val;

	ret = exynos_acpm_read_reg(S2MPU11_ADDR, slave, reg, &reg_val);
	*val = reg_val;

	if (ret)
		pr_err("[%s] acpm ipc read failed! err: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(aud3003x_acpm_read_reg);

int aud3003x_acpm_write_reg(unsigned int slave, unsigned int reg,
		unsigned int val)
{
	int ret;

	ret = exynos_acpm_write_reg(S2MPU11_ADDR, slave, reg, val);

	if (ret)
		pr_err("[%s] acpm ipc write failed! err: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(aud3003x_acpm_write_reg);

int aud3003x_acpm_update_reg(unsigned int slave, unsigned int reg,
		unsigned int val, unsigned int mask)
{
	int ret;

	ret = exynos_acpm_update_reg(S2MPU11_ADDR, slave, reg, val, mask);

	if (ret)
		pr_err("[%s] acpm ipc update failed! err: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(aud3003x_acpm_update_reg);

void regcache_cache_switch(struct aud3003x_priv *aud3003x, bool on)
{
	int p_map;

	if (!on)
		mutex_lock(&aud3003x->regcache_lock);
	else
		mutex_unlock(&aud3003x->regcache_lock);

	if (aud3003x->is_suspend)
		for (p_map = AUD3003D; p_map <= AUD3003O; p_map++)
			regcache_cache_only(aud3003x->regmap[p_map], on);
}

void i2c_client_change(struct aud3003x_priv *aud3003x, int client)
{
	struct device *dev = aud3003x->dev;
	struct i2c_client *i2c = to_i2c_client(dev);
	struct snd_soc_component *codec = aud3003x->codec;

	if (client != CODEC_CLOSE) {
		mutex_lock(&aud3003x->regmap_lock);
		i2c = aud3003x->i2c_priv[client];
		codec->regmap = aud3003x->regmap[client];
	} else {
		i2c = aud3003x->i2c_priv[AUD3003D];
		codec->regmap = aud3003x->regmap[AUD3003D];
		mutex_unlock(&aud3003x->regmap_lock);
	}
}

/*
 * Return Value
 * True: If the register value cannot be cached, hence we have to read from the
 * hardware directly.
 * False: If the register value can be read from cache.
 */
static bool aud3003x_volatile_register(struct device *dev, unsigned int reg)
{
	/*
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch (reg) {
	case AUD3003X_01_IRQ1PEND ... AUD3003X_06_IRQ6PEND:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be read
 * False: If the register cannot be read
 */
static bool aud3003x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUD3003X_01_IRQ1PEND ... AUD3003X_0F_IRQR2:
	case AUD3003X_10_CLKGATE0 ... AUD3003X_1F_CHOP2:
	case AUD3003X_20_IF_FORM1 ... AUD3003X_2F_DMIC_ST:
	case AUD3003X_30_ADC1 ... AUD3003X_3F_AD_OFFSETC:
	case AUD3003X_40_PLAY_MODE ... AUD3003X_7F_AVC48:
	case AUD3003X_80_ANA_INTR ... AUD3003X_8F_NOISE_INTR_NEG:
	case AUD3003X_93_DSM_CON1 ... AUD3003X_9E_AVC_BTS_ON_THRES:
	case AUD3003X_A0_AMU_CTR_CM1 ... AUD3003X_AD_AMU_CTR_HP6:
	case AUD3003X_B0_ODSEL1 ... AUD3003X_B9_ODSEL10:
	case AUD3003X_C0_ACTR_JD1 ... AUD3003X_CC_ACTR_DLDO:
	case AUD3003X_D0_DCTR_CM ... AUD3003X_EF_DCTR_CAL:
	case AUD3003X_F0_STATUS1 ... AUD3003X_FC_STATUS13:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be write
 * False: If the register cannot be write
 */
static bool aud3003x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUD3003X_08_IRQ1M ... AUD3003X_0D_IRQ6M:
	case AUD3003X_10_CLKGATE0 ... AUD3003X_1F_CHOP2:
	case AUD3003X_20_IF_FORM1 ... AUD3003X_2F_DMIC_ST:
	case AUD3003X_30_ADC1 ... AUD3003X_3F_AD_OFFSETC:
	case AUD3003X_40_PLAY_MODE ... AUD3003X_56_AVC7:
	case AUD3003X_58_AVC9 ... AUD3003X_74_AVC37:
	case AUD3003X_77_AVC40 ... AUD3003X_7F_AVC48:
	case AUD3003X_89_ANA_INTR_MASK ... AUD3003X_8E_OCP_INTR_MASK:
	case AUD3003X_93_DSM_CON1 ... AUD3003X_9A_DSM_BST_CON4:
	case AUD3003X_9D_AVC_DWA_OFF_THRES ... AUD3003X_9E_AVC_BTS_ON_THRES:
	case AUD3003X_A0_AMU_CTR_CM1 ... AUD3003X_AD_AMU_CTR_HP6:
	case AUD3003X_B0_ODSEL1 ... AUD3003X_B9_ODSEL10:
	case AUD3003X_C0_ACTR_JD1 ... AUD3003X_CC_ACTR_DLDO:
	case AUD3003X_D0_DCTR_CM ... AUD3003X_EF_DCTR_CAL:
		return true;
	default:
		return false;
	}
}

const struct regmap_config aud3003x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	/*
	 * "i3c" string should be described in the name field
	 * this will be used for the i3c inteface,
	 * when read/write operations are used in the regmap driver.
	 * APM functions will be called instead of the I2C
	 * refer to the "drivers/base/regmap/regmap-i2c.c
	 */
	.name = "i3c, AUD3003X",
	.max_register = AUD3003X_REGCACHE_SYNC_END,
	.readable_reg = aud3003x_readable_register,
	.writeable_reg = aud3003x_writeable_register,
	.volatile_reg = aud3003x_volatile_register,
	.use_single_rw = true,
	.cache_type = REGCACHE_RBTREE,
};

static bool aud3003x_volatile_analog_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	default:
		return false;
	}
}

static bool aud3003x_readable_analog_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUD3003X_102_REF_CTRL1 ... AUD3003X_10D_CTRL_VTS5:
	case AUD3003X_110_BST_PDB ... AUD3003X_125_DC_OFFSET_ON:
	case AUD3003X_130_PD_IDAC1 ... AUD3003X_131_PD_IDAC2:
	case AUD3003X_135_CLK_CTRL:
	case AUD3003X_138_PD_HP ... AUD3003X_13B_OCP_HP:
	case AUD3003X_13F_CT_HPL ... AUD3003X_140_CT_HPR:
	case AUD3003X_142_CTRL_OVP0 ... AUD3003X_145_CTRL_OVP3:
	case AUD3003X_149_PD_EP:
	case AUD3003X_14C_EP_AVOL:
	case AUD3003X_150_ADC_CLK0 ... AUD3003X_156_DA_CP3:
	case AUD3003X_180_READ_ON0 ... AUD3003X_18A_READ_ON10:
		return true;
	default:
		return false;
	}
}

static bool aud3003x_writeable_analog_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUD3003X_102_REF_CTRL1 ... AUD3003X_10D_CTRL_VTS5:
	case AUD3003X_110_BST_PDB ... AUD3003X_125_DC_OFFSET_ON:
	case AUD3003X_130_PD_IDAC1 ... AUD3003X_131_PD_IDAC2:
	case AUD3003X_135_CLK_CTRL:
	case AUD3003X_138_PD_HP ... AUD3003X_13B_OCP_HP:
	case AUD3003X_13F_CT_HPL ... AUD3003X_140_CT_HPR:
	case AUD3003X_142_CTRL_OVP0 ... AUD3003X_145_CTRL_OVP3:
	case AUD3003X_149_PD_EP:
	case AUD3003X_14C_EP_AVOL:
	case AUD3003X_150_ADC_CLK0 ... AUD3003X_156_DA_CP3:
		return true;
	default:
		return false;
	}
}

const struct regmap_config aud3003x_analog_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.name = "i3c, AUD3003X_ANALOG",
	.max_register = AUD3003X_REGCACHE_SYNC_END,
	.readable_reg = aud3003x_readable_analog_register,
	.writeable_reg = aud3003x_writeable_analog_register,
	.volatile_reg = aud3003x_volatile_analog_register,
	.use_single_rw = true,
	.cache_type = REGCACHE_RBTREE,
};

static bool aud3003x_volatile_otp_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	default:
		return false;
	}
}

static bool aud3003x_readable_otp_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUD3003X_200_HP_OFFSET_L0 ... AUD3003X_253_EP_OFFMSK_3:
	case AUD3003X_2A8_CTRL_IREF1 ... AUD3003X_2BF_EP_DSM_OFFSETR:
		return true;
	default:
		return false;
	}
}

static bool aud3003x_writeable_otp_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUD3003X_200_HP_OFFSET_L0 ... AUD3003X_253_EP_OFFMSK_3:
	case AUD3003X_2A8_CTRL_IREF1 ... AUD3003X_2BF_EP_DSM_OFFSETR:
		return true;
	default:
		return false;
	}
}

const struct regmap_config aud3003x_otp_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.name = "i3c, AUD3003X_OTP",
	.max_register = AUD3003X_REGCACHE_SYNC_END,
	.readable_reg = aud3003x_readable_otp_register,
	.writeable_reg = aud3003x_writeable_otp_register,
	.volatile_reg = aud3003x_volatile_otp_register,
	.use_single_rw = true,
	.cache_type = REGCACHE_RBTREE,
};

bool read_from_cache(struct device *dev, unsigned int reg, int map_type)
{
	bool result = false;

	switch (map_type) {
	case AUD3003D:
		result = aud3003x_readable_register(dev, reg) &&
			(!aud3003x_volatile_register(dev, reg));
		break;
	case AUD3003A:
		result = aud3003x_readable_analog_register(dev, reg) &&
			(!aud3003x_volatile_analog_register(dev, reg));
		break;
	case AUD3003O:
		result = aud3003x_readable_otp_register(dev, reg) &&
			(!aud3003x_volatile_otp_register(dev, reg));
		break;
	}

	return result;
}
EXPORT_SYMBOL_GPL(read_from_cache);

bool write_to_hw(struct device *dev, unsigned int reg, int map_type)
{
	bool result = false;

	switch (map_type) {
	case AUD3003D:
		result = aud3003x_writeable_register(dev, reg) &&
			(!aud3003x_volatile_register(dev, reg));
		break;
	case AUD3003A:
		result = aud3003x_writeable_analog_register(dev, reg) &&
			(!aud3003x_volatile_analog_register(dev, reg));
		break;
	case AUD3003O:
		result = aud3003x_writeable_otp_register(dev, reg) &&
			(!aud3003x_volatile_otp_register(dev, reg));
		break;
	}

	return result;
}

/*
 * TLV_DB_SCALE_ITEM (TLV: Threshold Limit Value)
 *
 * For various properties, the dB values don't change linearly with respect to
 * the digital value of related bit-field. At most, they are quasi-linear,
 * that means they are linear for various ranges of digital values. Following
 * table define such ranges of various properties.
 *
 * TLV_DB_RANGE_HEAD(num)
 * num defines the number of linear ranges of dB values.
 *
 * s0, e0, TLV_DB_SCALE_ITEM(min, step, mute),
 * s0: digital start value of this range (inclusive)
 * e0: digital end valeu of this range (inclusive)
 * min: dB value corresponding to s0
 * step: the delta of dB value in this range
 * mute: ?
 *
 * Example:
 *	TLV_DB_RANGE_HEAD(3),
 *	0, 1, TLV_DB_SCALE_ITEM(-2000, 2000, 0),
 *	2, 4, TLV_DB_SCALE_ITEM(1000, 1000, 0),
 *	5, 6, TLV_DB_SCALE_ITEM(3800, 8000, 0),
 *
 * The above code has 3 linear ranges with following digital-dB mapping.
 * (0...6) -> (-2000dB, 0dB, 1000dB, 2000dB, 3000dB, 3800dB, 4600dB),
 *
 * DECLARE_TLV_DB_SCALE
 *
 * This macro is used in case where there is a linear mapping between
 * the digital value and dB value.
 *
 * DECLARE_TLV_DB_SCALE(name, min, step, mute)
 *
 * name: name of this dB scale
 * min: minimum dB value corresponding to digital 0
 * step: the delta of dB value
 * mute: ?
 *
 * NOTE: The information is mostly for user-space consumption, to be viewed
 * alongwith amixer.
 */

/*
 * aud3003x_dmic_gain_tlv - Digital MIC gain
 *
 * Map as per data-sheet:
 * 000 ~ 111 : 1 to 15, step 2 code
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DMIC_GAIN_PRE : reg(0x32), shift(4), width(3), invert(0), max(7)
 */
static const unsigned int aud3003x_dmic_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x7, TLV_DB_SCALE_ITEM(100, 200, 0),
};

static int mic1_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool mic_boost_flag;

	i2c_client_change(aud3003x, AUD3003A);
	mic_boost_flag = snd_soc_component_read32(codec, AUD3003X_111_BST1) & CTVOL_BST_1_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = mic_boost_flag;

	return 0;
}

static int mic1_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	i2c_client_change(aud3003x, AUD3003A);
	if (value)
		snd_soc_component_update_bits(codec, AUD3003X_111_BST1,
				CTVOL_BST_MASK, CTVOL_BST_1_MASK);
	else
		snd_soc_component_update_bits(codec, AUD3003X_111_BST1,
				CTVOL_BST_MASK, CTVOL_BST_0_MASK);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}

static int mic2_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool mic_boost_flag;

	i2c_client_change(aud3003x, AUD3003A);
	mic_boost_flag = snd_soc_component_read32(codec, AUD3003X_112_BST2) & CTVOL_BST_1_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = mic_boost_flag;

	return 0;
}

static int mic2_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	i2c_client_change(aud3003x, AUD3003A);
	if (value)
		snd_soc_component_update_bits(codec, AUD3003X_112_BST2,
				CTVOL_BST_MASK, CTVOL_BST_1_MASK);
	else
		snd_soc_component_update_bits(codec, AUD3003X_112_BST2,
				CTVOL_BST_MASK, CTVOL_BST_0_MASK);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}

static int mic3_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool mic_boost_flag;

	i2c_client_change(aud3003x, AUD3003A);
	mic_boost_flag = snd_soc_component_read32(codec, AUD3003X_113_BST3) & CTVOL_BST_1_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = mic_boost_flag;

	return 0;
}

static int mic3_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	i2c_client_change(aud3003x, AUD3003A);
	if (value)
		snd_soc_component_update_bits(codec, AUD3003X_113_BST3,
				CTVOL_BST_MASK, CTVOL_BST_1_MASK);
	else
		snd_soc_component_update_bits(codec, AUD3003X_113_BST3,
				CTVOL_BST_MASK, CTVOL_BST_0_MASK);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}

static int mic4_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool mic_boost_flag;

	i2c_client_change(aud3003x, AUD3003A);
	mic_boost_flag = snd_soc_component_read32(codec, AUD3003X_114_BST4) & CTVOL_BST_1_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = mic_boost_flag;

	return 0;
}

static int mic4_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	i2c_client_change(aud3003x, AUD3003A);
	if (value)
		snd_soc_component_update_bits(codec, AUD3003X_114_BST4,
				CTVOL_BST_MASK, CTVOL_BST_1_MASK);
	else
		snd_soc_component_update_bits(codec, AUD3003X_114_BST4,
				CTVOL_BST_MASK, CTVOL_BST_0_MASK);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}

static int vts_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool mic_boost_flag;

	i2c_client_change(aud3003x, AUD3003A);
	mic_boost_flag = snd_soc_component_read32(codec, AUD3003X_10A_CTRL_VTS2) & CTVOL_VTS_BST_23_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = mic_boost_flag;

	return 0;
}

static int vts_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	i2c_client_change(aud3003x, AUD3003A);
	if (value)
		snd_soc_component_update_bits(codec, AUD3003X_10A_CTRL_VTS2,
				CTVOL_VTS_BST_MASK, 3 << CTVOL_VTS_BST_SHIFT);
	else
		snd_soc_component_update_bits(codec, AUD3003X_10A_CTRL_VTS2,
				CTVOL_VTS_BST_MASK, 0);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}

/*
 * aud3003x_dvol_adc_tlv - Digital volume for ADC
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE0 ~ 0xE5 : -70dB to -80dB, step 2.0dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_ADCL : reg(0x34), shift(0), width(8), invert(1), max(0xE5)
 * DVOL_ADCR : reg(0x35), shift(0), width(8), invert(1), max(0xE5)
 * DVOL_ADCC : reg(0x36), shift(0), width(8), invert(1), max(0xE5)
 */
static const unsigned int aud3003x_dvol_adc_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x00, 0x05, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x06, 0xE5, TLV_DB_SCALE_ITEM(-6950, 50, 0),
};

/*
 * aud3003x_dmic_st_tlv - DMIC strength selection
 *
 * Map as per data-sheet:
 * 0x0 ~ 0x3 : 0 to 3, step 1
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * CTVOL_AVC_PO_GAIN : reg(0x2F), shift(0), width(2), invert(0), max(0x3)
 */
static const unsigned int aud3003x_dmic_st_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x3, TLV_DB_SCALE_ITEM(0, 300, 0),
};

static const char * const aud3003x_hpf_sel_text[] = {
	"15Hz", "33Hz", "60Hz", "113Hz"
};

static SOC_ENUM_SINGLE_DECL(aud3003x_hpf_sel_enum, AUD3003X_37_AD_HPF,
		HPF_SEL_SHIFT, aud3003x_hpf_sel_text);

static const char * const aud3003x_hpf_order_text[] = {
	"2nd", "1st"
};

static SOC_ENUM_SINGLE_DECL(aud3003x_hpf_order_enum, AUD3003X_37_AD_HPF,
		HPF_ORDER_SHIFT, aud3003x_hpf_order_text);

static const char * const aud3003x_hpf_channel_text[] = {
	"Off", "On"
};

static SOC_ENUM_SINGLE_DECL(aud3003x_hpf_channel_enum_l, AUD3003X_37_AD_HPF,
		HPF_ENL_SHIFT, aud3003x_hpf_channel_text);

static SOC_ENUM_SINGLE_DECL(aud3003x_hpf_channel_enum_r, AUD3003X_37_AD_HPF,
		HPF_ENR_SHIFT, aud3003x_hpf_channel_text);

static SOC_ENUM_SINGLE_DECL(aud3003x_hpf_channel_enum_c, AUD3003X_37_AD_HPF,
		HPF_ENC_SHIFT, aud3003x_hpf_channel_text);

/*
 * aud3003x_adc_dat_src - I2S channel input data selection
 *
 * Map as per data-sheet:
 * 00 : ADC Left Channel Data
 * 01 : ADC Right Channel Data
 * 10 : ADC Center Channel Data
 * 11 : Zero data
 *
 * SEL_ADC0 : reg(0x23), shift(0), width(2)
 * SEL_ADC1 : reg(0x23), shift(2), width(2)
 * SEL_ADC2 : reg(0x23), shift(4), width(2)
 * SEL_ADC3 : reg(0x23), shift(6), width(2)
 */
static const char * const aud3003x_adc_dat_src[] = {
		"ADCL", "ADCR", "ADCC", "Zero"
};

static SOC_ENUM_SINGLE_DECL(aud3003x_adc_dat_enum0, AUD3003X_23_IF_FORM4,
		SEL_ADC0_SHIFT, aud3003x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(aud3003x_adc_dat_enum1,	AUD3003X_23_IF_FORM4,
		SEL_ADC1_SHIFT, aud3003x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(aud3003x_adc_dat_enum2,	AUD3003X_23_IF_FORM4,
		SEL_ADC2_SHIFT, aud3003x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(aud3003x_adc_dat_enum3, AUD3003X_23_IF_FORM4,
		SEL_ADC3_SHIFT, aud3003x_adc_dat_src);

static int dmic_bias_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool bias_flag;

	if (gpio_is_valid(aud3003x->dmic_bias_gpio)) {
		bias_flag = gpio_get_value(aud3003x->dmic_bias_gpio);
		dev_dbg(aud3003x->dev, "%s, dmic bias gpio: %d\n", __func__, bias_flag);

		if (bias_flag < 0)
			dev_err(aud3003x->dev, "%s, dmic bias gpio read error!\n", __func__);
		else
			ucontrol->value.integer.value[0] = bias_flag;
	} else {
		dev_err(aud3003x->dev, "%s, dmic bias gpio not valid!\n", __func__);
	}

	return 0;
}

static int dmic_bias_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (gpio_is_valid(aud3003x->dmic_bias_gpio)) {
		gpio_direction_output(aud3003x->dmic_bias_gpio, value);

		dev_dbg(aud3003x->dev, "%s: dmic bias : %s\n",
				__func__, (value) ? "On" : "Off");
	} else {
		dev_err(aud3003x->dev, "%s, dmic bias gpio not valid!\n", __func__);
	}

	return 0;
}

static int amic_bias_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool amic_bias_flag;

	i2c_client_change(aud3003x, AUD3003D);
	amic_bias_flag = snd_soc_component_read32(codec, AUD3003X_D2_DCTR_TEST2) & T_PDB_MCB_LDO_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = amic_bias_flag;

	return 0;
}

static int amic_bias_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	i2c_client_change(aud3003x, AUD3003D);
	if (value) {
		snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
				T_PDB_MCB_LDO_MASK, T_PDB_MCB_LDO_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_C5_ACTR_MCB3,
				PDB_MCB_LDO_MASK | PDB_MCB1_NS_MASK,
				PDB_MCB_LDO_MASK | PDB_MCB1_NS_MASK);
	} else {
		snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
				T_PDB_MCB_LDO_MASK, 0);
		/* PDB_MCB_LDO is always on */
		snd_soc_component_update_bits(codec, AUD3003X_C5_ACTR_MCB3,
				PDB_MCB1_NS_MASK, 0);
	}
	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}

static int hp_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aud3003x->hp_avc_gain;

	dev_err(aud3003x->dev, "%s, hp_avc_gain: %d\n",
			__func__, aud3003x->hp_avc_gain);

	return 0;
}

static int hp_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value < 0 || value > 14)
	{
		dev_err(aud3003x->dev, "%s, hp gain write error! (-14db ~ 0db)\n", __func__);
		value = 0;
	}

	aud3003x->hp_avc_gain = value;

	value = 14 - value;

	i2c_client_change(aud3003x, AUD3003D);

	snd_soc_component_write(codec, AUD3003X_7A_AVC43, 0x00);

	snd_soc_component_update_bits(codec, AUD3003X_5F_AVC16,
			SIGN_AV_MASK, 1 << SIGN_AV_SHIFT);
	snd_soc_component_update_bits(codec, AUD3003X_61_AVC18,
			SIGN_AC_MASK, 1 << SIGN_AC_SHIFT);
	snd_soc_component_update_bits(codec, AUD3003X_63_AVC20,
			SIGN_AN_MASK, 1 << SIGN_AN_SHIFT);

	snd_soc_component_update_bits(codec, AUD3003X_5F_AVC16,
			AVC_AVOL_VARI_MASK, value);
	snd_soc_component_update_bits(codec, AUD3003X_61_AVC18,
			AVC_AVOL_CONS_MASK, value);
	snd_soc_component_update_bits(codec, AUD3003X_63_AVC20,
			AVC_AVOL_NG_MASK, value);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	return 0;
}

static int rcv_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aud3003x->rcv_avc_gain;

	dev_err(aud3003x->dev, "%s, rcv_avc_gain: %d\n",
			__func__, aud3003x->rcv_avc_gain);

	return 0;
}

static int rcv_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value < 0 || value > 21)
	{
		dev_err(aud3003x->dev, "%s, rcv gain write error! (-14db ~ 7db)\n", __func__);
		value = 0;
	}

	aud3003x->rcv_avc_gain = value;

	i2c_client_change(aud3003x, AUD3003D);

	snd_soc_component_write(codec, AUD3003X_7A_AVC43, 0x00);

	if (value < 14)
	{
		snd_soc_component_update_bits(codec, AUD3003X_5F_AVC16,
				SIGN_AV_MASK, 1 << SIGN_AV_SHIFT);
		snd_soc_component_update_bits(codec, AUD3003X_61_AVC18,
				SIGN_AC_MASK, 1 << SIGN_AC_SHIFT);
		snd_soc_component_update_bits(codec, AUD3003X_63_AVC20,
				SIGN_AN_MASK, 1 << SIGN_AN_SHIFT);

		value = 14 - value;
	}
	else
	{
		snd_soc_component_update_bits(codec, AUD3003X_5F_AVC16,
				SIGN_AV_MASK, 0 << SIGN_AV_SHIFT);
		snd_soc_component_update_bits(codec, AUD3003X_61_AVC18,
				SIGN_AC_MASK, 0 << SIGN_AC_SHIFT);
		snd_soc_component_update_bits(codec, AUD3003X_63_AVC20,
				SIGN_AN_MASK, 0 << SIGN_AN_SHIFT);

		value = value - 14;
	}

	snd_soc_component_update_bits(codec, AUD3003X_5F_AVC16,
			AVC_AVOL_VARI_MASK, value);
	snd_soc_component_update_bits(codec, AUD3003X_61_AVC18,
			AVC_AVOL_CONS_MASK, value);
	snd_soc_component_update_bits(codec, AUD3003X_63_AVC20,
			AVC_AVOL_NG_MASK, value);

	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}
static int ovp_refp_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int ovp_refp_value;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003A);
	ovp_refp_value = snd_soc_component_read32(codec, AUD3003X_142_CTRL_OVP0) & CTRV_SURGE_REFP_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = ovp_refp_value >> CTRV_SURGE_REFP_SHIFT;

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int ovp_refp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value < 0)
		value = 0;
	else if (value > 7)
		value = 7;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003A);
	snd_soc_component_update_bits(codec, AUD3003X_142_CTRL_OVP0,
			CTRV_SURGE_REFP_MASK, value << CTRV_SURGE_REFP_SHIFT);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int ovp_refn_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int ovp_refn_value;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003A);
	ovp_refn_value = snd_soc_component_read32(codec, AUD3003X_142_CTRL_OVP0) & CTRV_SURGE_REFN_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = ovp_refn_value >> CTRV_SURGE_REFN_SHIFT;

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int ovp_refn_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value < 0)
		value = 0;
	else if (value > 15)
		value = 15;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003A);
	snd_soc_component_update_bits(codec, AUD3003X_142_CTRL_OVP0,
			CTRV_SURGE_REFN_MASK, value << CTRV_SURGE_REFN_SHIFT);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int gdet_vth_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int gdet_vth_value;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003D);
	gdet_vth_value = snd_soc_component_read32(codec, AUD3003X_C0_ACTR_JD1) & CTRV_GDET_VTG_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = gdet_vth_value >> CTRV_GDET_VTG_SHIFT;

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int gdet_vth_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value < 0)
		value = 0;
	else if (value > 7)
		value = 7;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003D);
	snd_soc_component_update_bits(codec, AUD3003X_C0_ACTR_JD1,
			CTRV_GDET_VTG_MASK, value << CTRV_GDET_VTG_SHIFT);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int ldet_vth_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int ldet_vth_value;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003D);
	ldet_vth_value = snd_soc_component_read32(codec, AUD3003X_C1_ACTR_JD2) & CTRV_LDET_VTH_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = ldet_vth_value >> CTRV_LDET_VTH_SHIFT;

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int ldet_vth_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value < 0)
		value = 0;
	else if (value > 3)
		value = 3;

	regcache_cache_switch(aud3003x, false);

	i2c_client_change(aud3003x, AUD3003D);
	snd_soc_component_update_bits(codec, AUD3003X_C1_ACTR_JD2,
			CTRV_LDET_VTH_MASK, value << CTRV_LDET_VTH_SHIFT);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	regcache_cache_switch(aud3003x, true);
	return 0;
}

static int aud3003x_digital_micbias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value;

	dev_err(aud3003x->dev, "%s: event = %d %d %d\n", __func__, event,
			SND_SOC_DAPM_PRE_PMU, SND_SOC_DAPM_POST_PMD);

	switch(event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (gpio_is_valid(aud3003x->dmic_bias_gpio)) {
			value = 1;
			gpio_direction_output(aud3003x->dmic_bias_gpio, value);
			dev_info(aud3003x->dev, "%s: dmic bias : %s\n",
					__func__, (value) ? "On" : "Off");
		} else {
			dev_err(aud3003x->dev, "%s, dmic bias gpio not valid!\n", __func__);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (gpio_is_valid(aud3003x->dmic_bias_gpio)) {
			value = 0;
			gpio_direction_output(aud3003x->dmic_bias_gpio, value);
			dev_info(aud3003x->dev, "%s: dmic bias : %s\n",
					__func__, (value) ? "On" : "Off");
		} else {
			dev_err(aud3003x->dev, "%s, dmic bias gpio not valid!\n", __func__);
		}
		break;
	}
	return 0;
}

/*
 * aud3003x_dvol_dac_tlv - Maximum headphone gain for EAR/RCV path
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE1 ~ 0xE5 : -72dB to -80dB, step 2.0dB
 * 0xE6 : -82.4dB
 * 0xE7 ~ 0xE9 : -84.3dB to -96.3dB, step 6dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_DAL : reg(0x41), shift(0), width(8), invert(1), max(0xE9)
 * DVOL_DAR : reg(0x42), shift(0), width(8), invert(1), max(0xE9)
 */
static const unsigned int aud3003x_dvol_dac_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x01, 0x03, TLV_DB_SCALE_ITEM(-9630, 600, 0),
	0x04, 0x04, TLV_DB_SCALE_ITEM(-8240, 0, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x0A, 0xE9, TLV_DB_SCALE_ITEM(-7000, 50, 0),
};

/*
 * aud3003x_ctvol_avc_tlv - AVC volume for EAR/RCV path
 *
 * Map as per data-sheet:
 * 0x0 ~ 0x7 : 0dB to 7dB, step 1dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * CTVOL_AVC_PO_GAIN : reg(0x53), shift(0), width(3), invert(0), max(0x7)
 */
static const unsigned int aud3003x_ctvol_avc_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x7, TLV_DB_SCALE_ITEM(0, 700, 0),
};

/*
 * aud3003x_dac_mixl_mode_text - DACL Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data L
 * 001 : (L+R)/2 Mono
 * 010 : (L+R) Mono
 * 011 : (L+R)/2 Polarity Changed
 * 100 : (L+R) Polarity Changed
 * 101 : Zero Padding
 * 110 : Data L Polarity Changed
 * 111 : Data R Polarity Changed
 *
 * DAC_MIXL : reg(0x44), shift(4), width(3)
 */
static const char * const aud3003x_dac_mixl_mode_text[] = {
		"Data L", "LR/2 Mono", "LR Mono", "LR/2 PolCh",
		"LR PolCh", "Zero" "Data L PolCh", "Data R PolCh"
};

static SOC_ENUM_SINGLE_DECL(aud3003x_dac_mixl_mode_enum, AUD3003X_44_PLAY_MIX0,
		DAC_MIXL_SHIFT, aud3003x_dac_mixl_mode_text);

/*
 * aud3003x_dac_mixr_mode_text - DACR Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data R
 * 001 : (L+R)/2 Mono
 * 010 : (L+R) Mono
 * 011 : (L+R)/2 Polarity Changed
 * 100 : (L+R) Polarity Changed
 * 101 : Zero Padding
 * 110 : Data R Polarity Changed
 * 111 : Data L Polarity Changed
 *
 * DAC_MIXR : reg(0x44), shift(0), width(3)
 */
static const char * const aud3003x_dac_mixr_mode_text[] = {
		"Data R", "LR/2 Mono", "LR Mono", "LR/2 PolCh",
		"LR PolCh", "Zero" "Data R PolCh", "Data L PolCh"
};

static SOC_ENUM_SINGLE_DECL(aud3003x_dac_mixr_mode_enum, AUD3003X_44_PLAY_MIX0,
		DAC_MIXR_SHIFT, aud3003x_dac_mixr_mode_text);

#ifdef CONFIG_SND_SOC_AUD3003X_HP_VTS
void aud3003x_hp_vts(struct aud3003x_priv *aud3003x, bool on)
{
	struct snd_soc_component *codec = aud3003x->codec;

	dev_dbg(codec->dev, "%s called. VTS ON: %s\n", __func__, on? "On" : "Off");

	regcache_cache_switch(aud3003x, false);
	i2c_client_change(aud3003x, AUD3003D);

	/* VTS HighZ Setting */
	if (on)
		snd_soc_component_update_bits(codec, AUD3003X_2F_DMIC_ST,
				VTS_DATA_ENB_MASK, 0);
	else
		snd_soc_component_update_bits(codec, AUD3003X_2F_DMIC_ST,
				VTS_DATA_ENB_MASK, VTS_DATA_ENB_MASK);

	/* Internal Clock On/Off */
	snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
			COM_CLK_GATE_MASK, COM_CLK_GATE_MASK);

	/* VTS Clock On/Off */
	snd_soc_component_update_bits(codec, AUD3003X_12_CLKGATE2,
			VTS_CLK_GATE_MASK, on << VTS_CLK_GATE_SHIFT);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	i2c_client_change(aud3003x, AUD3003A);

	/* Select whether to use amic or hp mic for VTS source */
	if (aud3003x->hp_vts_on)
		snd_soc_component_update_bits(codec, AUD3003X_10A_CTRL_VTS2,
				SEL_VTSIN_MASK, SEL_VTSIN_MASK);
	else
		snd_soc_component_update_bits(codec, AUD3003X_10A_CTRL_VTS2,
				SEL_VTSIN_MASK, 0);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	i2c_client_change(aud3003x, AUD3003D);

	/* VTS Power On/Off */
	snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
			APW_VTS_MASK, on << APW_VTS_SHIFT);
	snd_soc_component_update_bits(codec, AUD3003X_15_RESETB1,
			VTS_RESETB_MASK, on << VTS_RESETB_SHIFT);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	regcache_cache_switch(aud3003x, true);
}

static int hp_vts_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aud3003x->hp_vts_on;

	return 0;
}

static int hp_vts_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value) {
		aud3003x->hp_vts_on = true;
		aud3003x->amic_vts_on = false;
	} else {
		aud3003x->hp_vts_on = false;
	}

	aud3003x_hp_vts(aud3003x, aud3003x->hp_vts_on);

	dev_info(codec->dev, "%s: hp vts : %s\n",
			__func__, (value) ? "On" : "Off");

	return 0;
}

static int amic_vts_bias_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	bool amic_bias_flag, hq_flag;

	i2c_client_change(aud3003x, AUD3003D);
	amic_bias_flag = snd_soc_component_read32(codec, AUD3003X_D2_DCTR_TEST2) & T_PDB_MCB_LDO_MASK;
	hq_flag = snd_soc_component_read32(codec, AUD3003X_C3_ACTR_MCB1) & CTRV_MCB1_HQ_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	ucontrol->value.integer.value[0] = amic_bias_flag & (!hq_flag);

	return 0;
}

static int amic_vts_bias_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];
	bool suspend_on = false;

	if (aud3003x->is_suspend) {
		aud3003x_enable(codec->dev);
		suspend_on = true;
	}

	i2c_client_change(aud3003x, AUD3003D);
	if (value) {
		/* Turn off HQ mode for use PMIC BGR */
		snd_soc_component_update_bits(codec, AUD3003X_C3_ACTR_MCB1,
				CTRV_MCB1_HQ_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
				T_PDB_MCB_LDO_MASK, T_PDB_MCB_LDO_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_C5_ACTR_MCB3,
				PDB_MCB_LDO_MASK | PDB_MCB1_NS_MASK,
				PDB_MCB_LDO_MASK | PDB_MCB1_NS_MASK);
	} else {
		snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
				T_PDB_MCB_LDO_MASK, 0);
		/* PDB_MCB_LDO is always on */
		snd_soc_component_update_bits(codec, AUD3003X_C5_ACTR_MCB3,
				PDB_MCB1_NS_MASK, 0);
	}
	i2c_client_change(aud3003x, CODEC_CLOSE);

	if (suspend_on)
		aud3003x_disable(codec->dev);

	return 0;
}

static int amic_vts_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aud3003x->amic_vts_on;

	return 0;
}

static int amic_vts_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value) {
		aud3003x->hp_vts_on = false;
		aud3003x->amic_vts_on = true;
	} else {
		aud3003x->amic_vts_on = false;
	}

	aud3003x_hp_vts(aud3003x, aud3003x->amic_vts_on);

	dev_info(codec->dev, "%s: amic vts : %s\n",
			__func__, (value) ? "On" : "Off");

	return 0;
}
#endif

static int codec_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = !(aud3003x->is_suspend);

	return 0;
}

static int codec_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value)
		aud3003x_enable(codec->dev);
	else
		aud3003x_disable(codec->dev);

	dev_info(codec->dev, "%s: codec enable : %s\n",
			__func__, (value) ? "On" : "Off");

	return 0;
}

/*
 * struct snd_kcontrol_new aud3003x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/*
 * All the data goes into aud3003x_snd_controls.
 * All path inter-connections goes into aud3003x_dapm_routes
 */
static const struct snd_kcontrol_new aud3003x_snd_controls[] = {
	/*
	 * ADC(Tx) path control
	 */
	SOC_SINGLE_TLV("DMIC Gain", AUD3003X_32_ADC3,
			DMIC_GAIN_PRE_SHIFT,
			DMIC_GAIN_15, 0, aud3003x_dmic_gain_tlv),

	SOC_SINGLE_EXT("MIC1 Boost Gain", SND_SOC_NOPM, 0, 1, 0,
			mic1_boost_get, mic1_boost_put),

	SOC_SINGLE_EXT("MIC2 Boost Gain", SND_SOC_NOPM, 0, 1, 0,
			mic2_boost_get, mic2_boost_put),

	SOC_SINGLE_EXT("MIC3 Boost Gain", SND_SOC_NOPM, 0, 1, 0,
			mic3_boost_get, mic3_boost_put),

	SOC_SINGLE_EXT("MIC4 Boost Gain", SND_SOC_NOPM, 0, 1, 0,
			mic4_boost_get, mic4_boost_put),

	SOC_SINGLE_EXT("HPVTS Boost Gain", SND_SOC_NOPM, 0, 1, 0,
			vts_boost_get, vts_boost_put),

	SOC_SINGLE_TLV("ADC Left Gain", AUD3003X_34_AD_VOLL,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, aud3003x_dvol_adc_tlv),

	SOC_SINGLE_TLV("ADC Right Gain", AUD3003X_35_AD_VOLR,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, aud3003x_dvol_adc_tlv),

	SOC_SINGLE_TLV("ADC Center Gain", AUD3003X_36_AD_VOLC,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, aud3003x_dvol_adc_tlv),

	SOC_SINGLE_TLV("DMIC ST", AUD3003X_2F_DMIC_ST,
			DMIC_ST_SHIFT,
			DMIC_IO_ST_4, 0, aud3003x_dmic_st_tlv),

	SOC_SINGLE_EXT("DMIC Bias", SND_SOC_NOPM, 0, 1, 0,
			dmic_bias_get, dmic_bias_put),

	SOC_SINGLE_EXT("AMIC Bias", SND_SOC_NOPM, 0, 1, 0,
			amic_bias_get, amic_bias_put),

	SOC_ENUM("HPF Tuning", aud3003x_hpf_sel_enum),

	SOC_ENUM("HPF Order", aud3003x_hpf_order_enum),

	SOC_ENUM("HPF Left", aud3003x_hpf_channel_enum_l),

	SOC_ENUM("HPF Right", aud3003x_hpf_channel_enum_r),

	SOC_ENUM("HPF Center", aud3003x_hpf_channel_enum_c),

	SOC_ENUM("ADC DAT Mux0", aud3003x_adc_dat_enum0),

	SOC_ENUM("ADC DAT Mux1", aud3003x_adc_dat_enum1),

	SOC_ENUM("ADC DAT Mux2", aud3003x_adc_dat_enum2),

	SOC_ENUM("ADC DAT Mux3", aud3003x_adc_dat_enum3),

	/*
	 * DAC(Rx) path control
	 */
	SOC_DOUBLE_R_TLV("DAC Gain", AUD3003X_41_PLAY_VOLL, AUD3003X_42_PLAY_VOLR,
			DVOL_DA_SHIFT,
			DAC_DVOL_MINNUM, 1, aud3003x_dvol_dac_tlv),

	SOC_SINGLE_TLV("AVC Gain", AUD3003X_53_AVC4,
			CTVOL_AVC_PO_GAIN_SHIFT,
			CTVOL_AVC_GAIN_7, 0, aud3003x_ctvol_avc_tlv),

	SOC_SINGLE_EXT("HP AVC Gain", SND_SOC_NOPM, 0, 14, 0,
			hp_gain_get, hp_gain_put),

	SOC_SINGLE_EXT("RCV AVC Gain", SND_SOC_NOPM, 0, 14, 0,
			rcv_gain_get, rcv_gain_put),

	SOC_SINGLE_EXT("OVP REFP", SND_SOC_NOPM, 0, 7, 0,
			ovp_refp_get, ovp_refp_put),

	SOC_SINGLE_EXT("OVP REFN", SND_SOC_NOPM, 0, 15, 0,
			ovp_refn_get, ovp_refn_put),

	SOC_SINGLE_EXT("GDET VTH", SND_SOC_NOPM, 0, 7, 0,
			gdet_vth_get, gdet_vth_put),

	SOC_SINGLE_EXT("LDET VTH", SND_SOC_NOPM, 0, 3, 0,
			ldet_vth_get, ldet_vth_put),

	SOC_ENUM("DAC_MIXL Mode", aud3003x_dac_mixl_mode_enum),

	SOC_ENUM("DAC_MIXR Mode", aud3003x_dac_mixr_mode_enum),

#ifdef CONFIG_SND_SOC_AUD3003X_HP_VTS
	SOC_SINGLE_EXT("HPVTS On", SND_SOC_NOPM, 0, 1, 0,
			hp_vts_get, hp_vts_put),

	SOC_SINGLE_EXT("AMICVTS Bias", SND_SOC_NOPM, 0, 1, 0,
			amic_vts_bias_get, amic_vts_bias_put),

	SOC_SINGLE_EXT("AMICVTS On", SND_SOC_NOPM, 0, 1, 0,
			amic_vts_get, amic_vts_put),
#endif

	/*
	 * Jack control
	 */
	SOC_SINGLE("Jack DBNC In", AUD3003X_D8_DCTR_DBNC1,
			A2D_JACK_DBNC_IN_SHIFT, A2D_JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack DBNC Out", AUD3003X_D8_DCTR_DBNC1,
			A2D_JACK_DBNC_OUT_SHIFT, A2D_JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack DBNC IRQ In", AUD3003X_D9_DCTR_DBNC2,
			A2D_JACK_DBNC_INT_IN_SHIFT, A2D_JACK_DBNC_INT_MAX, 0),

	SOC_SINGLE("Jack DBNC IRQ Out", AUD3003X_D9_DCTR_DBNC2,
			A2D_JACK_DBNC_INT_OUT_SHIFT, A2D_JACK_DBNC_INT_MAX, 0),

	/*
	 * Codec control
	 */
	SOC_SINGLE_EXT("Codec Enable", SND_SOC_NOPM, 0, 1, 0,
			codec_enable_get, codec_enable_put),
};

/*
 * ADC(Tx) functions
 */

/*
 * aud3003x_mic_bias_ev() - Set micbias at machine driver
 *
 * @codec: SoC audio codec device
 * @mic_bias: Select mic bias
 * @event: dapm event types
 *
 * Desc: If the machine driver select the source of mic bias,
 * provide the function for using mic bias of aud3003x codec.
 */
int aud3003x_mic_bias_ev(struct snd_soc_component *codec, int mic_bias, int event)
{
#if 0
	int is_other_mic_on, mask;

	dev_dbg(codec->dev, "%s Called, Mic bias = %d, Event = %d\n",
				__func__, mic_bias, event);

	is_other_mic_on = snd_soc_component_read32(codec, AUD3003X_10_PD_REF);

	if (mic_bias == AUD3003X_MICBIAS1) {
		is_other_mic_on &= PDB_MCB2_CODEC_MASK;
		mask = is_other_mic_on ? PDB_MCB1_MASK :
			PDB_MCB1_MASK | PDB_MCB_LDO_CODEC_MASK;
	} else if (mic_bias == AUD3003X_MICBIAS2) {
		is_other_mic_on &= PDB_MCB1_MASK;
		mask = is_other_mic_on ? PDB_MCB2_CODEC_MASK :
			PDB_MCB2_CODEC_MASK | PDB_MCB_LDO_CODEC_MASK;
	} else {
		dev_err(codec->dev, "%s Called , Invalid MIC ID\n", __func__);
		return -1;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (mic_bias == AUD3003X_MICBIAS2)
			snd_soc_component_update_bits(codec, AUD3003X_19_CTRL_REF,
					CTRM_MCB2_MASK, CTRM_MCB2_MASK);
		else
			snd_soc_component_update_bits(codec,
					AUD3003X_10_PD_REF, mask, mask);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (mic_bias == AUD3003X_MICBIAS2)
			snd_soc_component_update_bits(codec, AUD3003X_19_CTRL_REF,
					CTRM_MCB2_MASK, 0);
		else
			snd_soc_component_update_bits(codec, AUD3003X_10_PD_REF, mask, 0);
		break;
	}
#endif
	return 0;
}

/*
 * aud3003x_adc_digital_mute() - Set ADC digital Mute
 *
 * @codec: SoC audio codec device
 * @channel: Digital mute control for ADC channel
 * @on: mic mute is true, mic unmute is false
 *
 * Desc: When ADC path turn on, analog block noise can be recorded.
 * For remove this, ADC path was muted always except that it was used.
 */
void aud3003x_adc_digital_mute(struct snd_soc_component *codec,
		unsigned int channel, bool on)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	mutex_lock(&aud3003x->adc_mute_lock);

	if (on) {
		i2c_client_change(aud3003x, AUD3003D);
		snd_soc_component_update_bits(codec, AUD3003X_30_ADC1, channel, channel);
		i2c_client_change(aud3003x, CODEC_CLOSE);
	} else {
		i2c_client_change(aud3003x, AUD3003D);
		snd_soc_component_update_bits(codec, AUD3003X_30_ADC1, channel, 0);
		i2c_client_change(aud3003x, CODEC_CLOSE);
	}

	mutex_unlock(&aud3003x->adc_mute_lock);

	dev_dbg(codec->dev, "%s called, %s work done.\n",
			__func__, on ? "Mute" : "Unmute");
}

static void aud3003x_amic_dsm_reset(struct aud3003x_priv *aud3003x,
		unsigned int mic_type)
{
	struct snd_soc_component *codec = aud3003x->codec;

	dev_dbg(codec->dev, "%s called, mic type: %d\n", __func__, mic_type);

	i2c_client_change(aud3003x, AUD3003D);
	if (mic_type & MIC1_ON_MASK)
		snd_soc_component_update_bits(codec, AUD3003X_B4_ODSEL5,
				T_RESETB_INTL_MASK | T_RESETB_QUANTL_MASK,
				T_RESETB_INTL_MASK | T_RESETB_QUANTL_MASK);
	if (mic_type & MIC2_ON_MASK)
		snd_soc_component_update_bits(codec, AUD3003X_B4_ODSEL5,
				T_RESETB_INTC_MASK | T_RESETB_QUANTC_MASK,
				T_RESETB_INTC_MASK | T_RESETB_QUANTC_MASK);
	if (mic_type & (MIC3_ON_MASK | MIC4_ON_MASK))
		snd_soc_component_update_bits(codec, AUD3003X_B5_ODSEL6,
				T_RESETB_INTR_MASK | T_RESETB_QUANTR_MASK,
				T_RESETB_INTR_MASK | T_RESETB_QUANTR_MASK);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	aud3003x_usleep(500);

	i2c_client_change(aud3003x, AUD3003D);
	if (mic_type & MIC1_ON_MASK)
		snd_soc_component_update_bits(codec, AUD3003X_B4_ODSEL5,
				T_RESETB_INTL_MASK | T_RESETB_QUANTL_MASK, 0);
	if (mic_type & MIC2_ON_MASK)
		snd_soc_component_update_bits(codec, AUD3003X_B4_ODSEL5,
				T_RESETB_INTC_MASK | T_RESETB_QUANTC_MASK, 0);
	if (mic_type & (MIC3_ON_MASK | MIC4_ON_MASK))
		snd_soc_component_update_bits(codec, AUD3003X_B5_ODSEL6,
				T_RESETB_INTR_MASK | T_RESETB_QUANTR_MASK, 0);
	i2c_client_change(aud3003x, CODEC_CLOSE);
}

static void aud3003x_adc_mute_work(struct work_struct *work)
{
	struct aud3003x_priv *aud3003x =
		container_of(work, struct aud3003x_priv, adc_mute_work.work);
	struct snd_soc_component *codec = aud3003x->codec;
	unsigned int amic_on;

	i2c_client_change(aud3003x, AUD3003D);
	amic_on = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1) & AMIC_ON_MASK;
	i2c_client_change(aud3003x, CODEC_CLOSE);

	dev_dbg(codec->dev, "%s called, amic_on = 0x%02x\n", __func__, amic_on);

	if (amic_on) {
		aud3003x_amic_dsm_reset(aud3003x, amic_on);
		if (amic_on & MIC4_ON_MASK)
			reset_mic4_boost(aud3003x->p_jackdet);
		msleep(50);
	}

	if (aud3003x->capture_on)
		aud3003x_adc_digital_mute(codec, ADC_MUTE_ALL, false);
}

#if 0
/*
 * aud3003x_get_mic_status() - Return the mic enable bits
 *
 * @codec: SoC audio codec device
 *
 * Desc:
 */
static unsigned int aud3003x_get_mic_status(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called, mic_status: 0x%x\n",
			__func__, aud3003x->mic_status);

	return aud3003x->mic_status;
}

/*
 * aud3003x_set_mic_status() - Set mic status with bits
 *
 * @codec: SoC audio codec device
 * @on: mic on is true, mic off is false
 *
 * Desc:
 */
static void aud3003x_set_mic_status(struct snd_soc_component *codec,
		unsigned int mic, bool on)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	if (on) {
		aud3003x->mic_status |= mic;
		dev_dbg(codec->dev, "%s called, mic_status on: 0x%x\n",
				__func__, aud3003x->mic_status);
	} else {
		aud3003x->mic_status &= ~mic;
		dev_dbg(codec->dev, "%s called, mic_status off: 0x%x\n",
				__func__, aud3003x->mic_status);
	}
}
#endif

static int vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, play_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	play_on = chop_val & (HP_ON_MASK | EP_ON_MASK);

	dev_dbg(codec->dev, "%s called, chop = 0x%02x, event = %d\n",
			__func__, chop_val, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Clock enable for ADC */
		snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
				SEQ_CLK_GATE_MASK | ADC_CLK_GATE_MASK,
				CLKGATE0_ENABLE << SEQ_CLK_GATE_SHIFT |
				CLKGATE0_ENABLE << ADC_CLK_GATE_SHIFT);

		/* Clock enable for Internal Filter */
		snd_soc_component_update_bits(codec, AUD3003X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				CLKGATE2_ENABLE << ADC_CIC_CGL_SHIFT |
				CLKGATE2_ENABLE << ADC_CIC_CGR_SHIFT);

		/* ADC Gain Trim */
		snd_soc_component_write(codec, AUD3003X_38_AD_TRIM1, 0x05);
		snd_soc_component_write(codec, AUD3003X_39_AD_TRIM2, 0x03);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003A);

		/* DWA (Rev 0.1: On, Rev 0.0: Off) */
		snd_soc_component_update_bits(codec, AUD3003X_124_AUTO_DW,
				AUTO_DWAL_MASK | AUTO_DWAC_MASK | AUTO_DWAR_MASK,
				AUTO_DWAL_MASK | AUTO_DWAC_MASK | AUTO_DWAR_MASK);
		if (aud3003x->codec_ver == REV_0_1)
			snd_soc_component_update_bits(codec, AUD3003X_11B_DSMC,
					EN_DWAL_MASK | EN_DWAC_MASK | EN_DWAR_MASK,
					EN_DWAL_MASK | EN_DWAC_MASK | EN_DWAR_MASK);
		else
			snd_soc_component_update_bits(codec, AUD3003X_11B_DSMC,
					EN_DWAL_MASK | EN_DWAC_MASK | EN_DWAR_MASK, 0);

		/* CTMI VCM */
		snd_soc_component_write(codec, AUD3003X_120_CTRL_MIC_I3, 0x5D);

		/* LCP CLK speed */
		snd_soc_component_update_bits(codec, AUD3003X_151_ADC_CLK1,
				LCP4_CLK_SEL_MASK | LCP1_CLK_SEL_MASK,
				CLK_SEL_0_78125M << LCP4_CLK_SEL_SHIFT |
				CLK_SEL_0_78125M << LCP1_CLK_SEL_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Compensation Filter */
		switch (aud3003x->capture_aifrate) {
		case AUD3003X_SAMPLE_RATE_48KHZ:
			snd_soc_component_update_bits(codec, AUD3003X_33_ADC4, CP_TYPE_SEL_MASK,
				FILTER_TYPE_NORMAL_AMIC << CP_TYPE_SEL_SHIFT);
			break;
		case AUD3003X_SAMPLE_RATE_192KHZ:
		case AUD3003X_SAMPLE_RATE_384KHZ:
			snd_soc_component_update_bits(codec, AUD3003X_33_ADC4, CP_TYPE_SEL_MASK,
				FILTER_TYPE_UHQA_WO_LP_AMIC << CP_TYPE_SEL_SHIFT);
		}

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* Compensation Filter off */
		snd_soc_component_update_bits(codec, AUD3003X_33_ADC4, CP_TYPE_SEL_MASK,
				FILTER_TYPE_NORMAL_AMIC << CP_TYPE_SEL_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003A);

		/* LCP CLK speed default set */
		snd_soc_component_update_bits(codec, AUD3003X_151_ADC_CLK1,
				LCP4_CLK_SEL_MASK | LCP1_CLK_SEL_MASK, 0);

		/* CTMI VCM default set */
		snd_soc_component_write(codec, AUD3003X_120_CTRL_MIC_I3, 0x49);

		/* DWA default set */
		snd_soc_component_update_bits(codec, AUD3003X_124_AUTO_DW,
				AUTO_DWAL_MASK | AUTO_DWAC_MASK | AUTO_DWAR_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_11B_DSMC,
				EN_DWAL_MASK | EN_DWAC_MASK | EN_DWAR_MASK,
				EN_DWAL_MASK | EN_DWAC_MASK | EN_DWAR_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003D);

		/* Clock disable for Internal Filter */
		snd_soc_component_update_bits(codec, AUD3003X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				CLKGATE2_DISABLE << ADC_CIC_CGL_SHIFT |
				CLKGATE2_DISABLE << ADC_CIC_CGR_SHIFT);

		/* Clock disable for ADC */
		snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
				ADC_CLK_GATE_MASK, 0);
		if (!play_on)
			snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int dvmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Clock enable for ADC */
		snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
				DMIC_CLK_GATE_MASK | ADC_CLK_GATE_MASK,
				CLKGATE0_ENABLE << DMIC_CLK_GATE_SHIFT |
				CLKGATE0_ENABLE << ADC_CLK_GATE_SHIFT);

		/* Clock enable for Internal Filter */
		snd_soc_component_update_bits(codec, AUD3003X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				CLKGATE2_ENABLE << ADC_CIC_CGL_SHIFT |
				CLKGATE2_ENABLE << ADC_CIC_CGR_SHIFT);

		/* DMIC CLK ZTIE disble */
		snd_soc_component_update_bits(codec, AUD3003X_32_ADC3,
				DMIC_CLK_ZTIE_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_32_ADC3,
				DMIC_GAIN_PRE_MASK, DMIC_GAIN_1 << DMIC_GAIN_PRE_SHIFT);

		/* ADC Gain Trim */
		snd_soc_component_write(codec, AUD3003X_38_AD_TRIM1, 0x0C);
		snd_soc_component_write(codec, AUD3003X_39_AD_TRIM2, 0x77);

		/* DMIC strength enable */
		snd_soc_component_update_bits(codec, AUD3003X_31_ADC2,
				EN_DMIC_MASK, DMIC_ENABLE << EN_DMIC_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Compensation Filter/OSR */
		switch (aud3003x->capture_aifrate) {
		case AUD3003X_SAMPLE_RATE_48KHZ:
			snd_soc_component_update_bits(codec, AUD3003X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_NORMAL_DMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_64 << DMIC_OSR_SHIFT);
			break;
		case AUD3003X_SAMPLE_RATE_192KHZ:
		case AUD3003X_SAMPLE_RATE_384KHZ:
			snd_soc_component_update_bits(codec, AUD3003X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_UHQA_WO_LP_DMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_64 << DMIC_OSR_SHIFT);
			break;
		}

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* Compensation Filter/OSR off */
		snd_soc_component_update_bits(codec, AUD3003X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_NORMAL_AMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_NO << DMIC_OSR_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* ADC Gain Trim clear */
		snd_soc_component_write(codec, AUD3003X_38_AD_TRIM1, 0x05);
		snd_soc_component_write(codec, AUD3003X_39_AD_TRIM2, 0x03);

		/* Clock disable for Internal Filter */
		snd_soc_component_update_bits(codec, AUD3003X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				CLKGATE2_DISABLE << ADC_CIC_CGL_SHIFT |
				CLKGATE2_DISABLE << ADC_CIC_CGR_SHIFT);

		/* Clock disable for ADC */
		snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
				DMIC_CLK_GATE_MASK | ADC_CLK_GATE_MASK,
				CLKGATE0_DISABLE << DMIC_CLK_GATE_SHIFT |
				CLKGATE0_DISABLE << ADC_CLK_GATE_SHIFT);

		/* DMIC CLK ZTIE enable */
		snd_soc_component_update_bits(codec, AUD3003X_32_ADC3,
				DMIC_CLK_ZTIE_MASK, DMIC_CLK_ZTIE_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_32_ADC3,
				DMIC_GAIN_PRE_MASK, DMIC_GAIN_7 << DMIC_GAIN_PRE_SHIFT);

		/* DMIC strength disable */
		snd_soc_component_update_bits(codec, AUD3003X_31_ADC2,
				EN_DMIC_MASK, DMIC_DISABLE << EN_DMIC_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static void aud3003x_amic_cap_cal(struct aud3003x_priv *aud3003x, int mic_type)
{
	struct snd_soc_component *codec = aud3003x->codec;
	int amicl_cap_cal, amicc_cap_cal, amicr_cap_cal;
	unsigned int chop_val, mic_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	mic_on = chop_val & AMIC_ON_MASK;

	dev_dbg(codec->dev, "%s called, codec rev: 0.%d mic type: %d\n",
			__func__, aud3003x->codec_ver, mic_type);

	mutex_lock(&aud3003x->cap_cal_lock);

	if (!aud3003x->amic_cal_done) {
		/* First time of amic work */
		aud3003x->amic_cal_done = true;
		aud3003x_usleep(5000);

		/* Calibration code read */
		i2c_client_change(aud3003x, AUD3003A);

		regcache_cache_bypass(aud3003x->regmap[AUD3003A], true);
		amicl_cap_cal = snd_soc_component_read32(codec, AUD3003X_184_READ_ON4);
		amicl_cap_cal &= CAP_SWL_MASK;
		amicc_cap_cal = snd_soc_component_read32(codec, AUD3003X_185_READ_ON5);
		amicc_cap_cal &= CAP_SWC_MASK;
		amicr_cap_cal = snd_soc_component_read32(codec, AUD3003X_186_READ_ON6);
		amicr_cap_cal &= CAP_SWR_MASK;
		regcache_cache_bypass(aud3003x->regmap[AUD3003A], false);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		/* Cal code calibration */
		dev_dbg(codec->dev, "%s ADCL:%d ADCC:%d ADCR:%d cal code read.\n",
				__func__, amicl_cap_cal, amicc_cap_cal, amicr_cap_cal);
		amicl_cap_cal += 2;
		if (amicl_cap_cal >= 31)
			amicl_cap_cal = 31;
		amicc_cap_cal += 2;
		if (amicc_cap_cal >= 31)
			amicc_cap_cal = 31;
		amicr_cap_cal += 2;
		if (amicr_cap_cal >= 31)
			amicr_cap_cal = 31;

		aud3003x->amic_cal[0] = amicl_cap_cal;
		aud3003x->amic_cal[1] = amicc_cap_cal;
		aud3003x->amic_cal[2] = amicr_cap_cal;

		i2c_client_change(aud3003x, AUD3003A);

		/* DSM ISI SW On */
		snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, PDB_DSML_MASK, PDB_DSML_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_117_DSM2, PDB_DSMC_MASK, PDB_DSMC_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_119_DSM3, PDB_DSMR_MASK, PDB_DSMR_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003O);

		/* MAN_CAP write */
		snd_soc_component_update_bits(codec, AUD3003X_2AB_CTRL_ADC1, CAP_SWL_MASK, amicl_cap_cal);
		snd_soc_component_update_bits(codec, AUD3003X_2AC_CTRL_ADC2, CAP_SWC_MASK, amicc_cap_cal);
		snd_soc_component_update_bits(codec, AUD3003X_2AD_CTRL_ADC3, CAP_SWR_MASK, amicr_cap_cal);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003A);

		/* CAL_SKIP On */
		snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, CAL_SKIP_MASK, CAL_SKIP_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003D);

		/* Mic power off */
		if (!(mic_on & MIC1_ON_MASK))
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD, APW_MIC1L_MASK, 0);
		if (!(mic_on & MIC2_ON_MASK))
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD, APW_MIC2C_MASK, 0);
		if (!(mic_on & MIC3_ON_MASK))
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD, APW_MIC3R_MASK, 0);
		if (!(mic_on & MIC4_ON_MASK))
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD, APW_MIC4R_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
	} else {
		dev_dbg(codec->dev, "%s ADC cal done already, ADCL:%d ADCC:%d ADCR:%d\n",
				__func__, aud3003x->amic_cal[0], aud3003x->amic_cal[1], aud3003x->amic_cal[2]);
	}

	i2c_client_change(aud3003x, AUD3003A);

	switch (mic_type) {
	case AMIC_L:
		/* DC offset on */
		snd_soc_component_update_bits(codec, AUD3003X_125_DC_OFFSET_ON,
				DC_OFFSET_LCH_1ST_MASK | DC_OFFSET_LCH_2ND_MASK,
				DC_OFFSET_LCH_1ST_MASK | DC_OFFSET_LCH_2ND_MASK);
		break;
	case AMIC_C:
		/* DC offset on */
		snd_soc_component_update_bits(codec, AUD3003X_125_DC_OFFSET_ON,
				DC_OFFSET_CCH_1ST_MASK | DC_OFFSET_CCH_2ND_MASK,
				DC_OFFSET_CCH_1ST_MASK | DC_OFFSET_CCH_2ND_MASK);
		break;
	case AMIC_R:
		/* DC offset on */
		snd_soc_component_update_bits(codec, AUD3003X_125_DC_OFFSET_ON,
				DC_OFFSET_RCH_1ST_MASK | DC_OFFSET_RCH_2ND_MASK,
				DC_OFFSET_RCH_1ST_MASK | DC_OFFSET_RCH_2ND_MASK);
		break;
	}

	/* CAL_SKIP On */
	snd_soc_component_update_bits(codec, AUD3003X_115_DSM1,
			CAL_SKIP_MASK, CAL_SKIP_MASK);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	mutex_unlock(&aud3003x->cap_cal_lock);
}

static int mic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic_on, play_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	play_on = chop_val & (HP_ON_MASK | EP_ON_MASK);
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(mic_on & MIC1_ON_MASK)) {
		dev_dbg(codec->dev, "%s: MIC1 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003A);

		/* LPF On */
		snd_soc_component_update_bits(codec, AUD3003X_111_BST1,
				CTMF_LPF_MASK, CTMF_LPF_53K << CTMF_LPF_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			i2c_client_change(aud3003x, AUD3003A);

			/* CAL_SKIP Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, CAL_SKIP_MASK, 0);

			/* DSM ISI SW Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, PDB_DSML_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_117_DSM2, PDB_DSMC_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_119_DSM3, PDB_DSMR_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			/* All mic power on */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK);
		} else {
			/* Mic1 Auto Power On */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC1L_MASK, AD_APW_ENABLE << APW_MIC1L_SHIFT);
		}

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1)
			aud3003x_amic_cap_cal(aud3003x, AMIC_L);
		else
			snd_soc_component_update_bits(codec, AUD3003X_2AB_CTRL_ADC1, CAP_SWL_MASK, 0x10);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (aud3003x->codec_ver == REV_0_1) {
			i2c_client_change(aud3003x, AUD3003A);

			/* DC offset off */
			snd_soc_component_update_bits(codec, AUD3003X_125_DC_OFFSET_ON,
					DC_OFFSET_LCH_1ST_MASK | DC_OFFSET_LCH_2ND_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

		/* Mic1 Auto Power Off */
		snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
				APW_MIC1L_MASK, AD_APW_DISABLE << APW_MIC1L_SHIFT);

		/* Reset off ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		if (play_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003A);

		/* LPF Off */
		snd_soc_component_update_bits(codec, AUD3003X_111_BST1,
				CTMF_LPF_MASK, CTMF_LPF_OFF << CTMF_LPF_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int mic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic_on, play_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	play_on = chop_val & (HP_ON_MASK | EP_ON_MASK);
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(mic_on & MIC2_ON_MASK)) {
		dev_dbg(codec->dev, "%s: MIC2 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003A);

		/* LPF On */
		snd_soc_component_update_bits(codec, AUD3003X_112_BST2,
				CTMF_LPF_MASK, CTMF_LPF_53K << CTMF_LPF_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			i2c_client_change(aud3003x, AUD3003A);

			/* CAL_SKIP Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, CAL_SKIP_MASK, 0);

			/* DSM ISI SW Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, PDB_DSML_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_117_DSM2, PDB_DSMC_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_119_DSM3, PDB_DSMR_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			/* All mic power on */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK);
		} else {
			/* Mic2 Auto Power On */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC2C_MASK, AD_APW_ENABLE << APW_MIC2C_SHIFT);
		}

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1)
			aud3003x_amic_cap_cal(aud3003x, AMIC_C);
		else
			snd_soc_component_update_bits(codec, AUD3003X_2AC_CTRL_ADC2,
					CAP_SWC_MASK, 0x10);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (aud3003x->codec_ver == REV_0_1) {
			i2c_client_change(aud3003x, AUD3003A);

			/* DC offset off */
			snd_soc_component_update_bits(codec, AUD3003X_125_DC_OFFSET_ON,
					DC_OFFSET_CCH_1ST_MASK | DC_OFFSET_CCH_2ND_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

		/* Mic2 Auto Power Off */
		snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
				APW_MIC2C_MASK, AD_APW_DISABLE << APW_MIC2C_SHIFT);

		/* Reset off ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		if (play_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003A);

		/* LPF Off */
		snd_soc_component_update_bits(codec, AUD3003X_112_BST2,
				CTMF_LPF_MASK, CTMF_LPF_OFF << CTMF_LPF_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int mic3_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic_on, play_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	play_on = chop_val & (HP_ON_MASK | EP_ON_MASK);
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(mic_on & MIC3_ON_MASK)) {
		dev_dbg(codec->dev, "%s: MIC3 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003A);

		/* MIC34 selection */
		snd_soc_component_update_bits(codec, AUD3003X_11D_BST_OP, SEL_MIC34_MASK, 0);

		/* LPF On */
		snd_soc_component_update_bits(codec, AUD3003X_113_BST3,
				CTMF_LPF_MASK, CTMF_LPF_53K << CTMF_LPF_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			i2c_client_change(aud3003x, AUD3003A);

			/* CAL_SKIP Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, CAL_SKIP_MASK, 0);

			/* DSM ISI SW Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, PDB_DSML_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_117_DSM2, PDB_DSMC_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_119_DSM3, PDB_DSMR_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			/* All mic power on */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK);
		} else {
			/* Mic3 Auto Power On */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC3R_MASK, AD_APW_ENABLE << APW_MIC3R_SHIFT);
		}

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1)
			aud3003x_amic_cap_cal(aud3003x, AMIC_R);
		else
			snd_soc_component_update_bits(codec, AUD3003X_2AD_CTRL_ADC3,
					CAP_SWR_MASK, 0x10);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (aud3003x->codec_ver == REV_0_1) {
			i2c_client_change(aud3003x, AUD3003A);

			/* DC offset off */
			snd_soc_component_update_bits(codec, AUD3003X_125_DC_OFFSET_ON,
					DC_OFFSET_RCH_1ST_MASK | DC_OFFSET_RCH_2ND_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

		/* Mic3 Auto Power Off */
		snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
				APW_MIC3R_MASK, AD_APW_DISABLE << APW_MIC3R_SHIFT);

		/* Reset off ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		if (play_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003A);

		/* LPF Off */
		snd_soc_component_update_bits(codec, AUD3003X_113_BST3,
				CTMF_LPF_MASK, CTMF_LPF_OFF << CTMF_LPF_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int mic4_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic_on, play_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	play_on = chop_val & (HP_ON_MASK | EP_ON_MASK);
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(mic_on & MIC4_ON_MASK)) {
		dev_dbg(codec->dev, "%s: MIC4 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003A);

		/* MIC34 selection */
		snd_soc_component_update_bits(codec, AUD3003X_11D_BST_OP,
				SEL_MIC34_MASK, SEL_MIC34_MASK);

		/* LPF On */
		snd_soc_component_update_bits(codec, AUD3003X_114_BST4,
				CTMF_LPF_MASK, CTMF_LPF_53K << CTMF_LPF_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			i2c_client_change(aud3003x, AUD3003A);

			/* CAL_SKIP Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, CAL_SKIP_MASK, 0);

			/* DSM ISI SW Off */
			snd_soc_component_update_bits(codec, AUD3003X_115_DSM1, PDB_DSML_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_117_DSM2, PDB_DSMC_MASK, 0);
			snd_soc_component_update_bits(codec, AUD3003X_119_DSM3, PDB_DSMR_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

		if (aud3003x->codec_ver == REV_0_1 && !aud3003x->amic_cal_done) {
			/* All mic power on */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK,
					APW_MIC1L_MASK | APW_MIC2C_MASK | APW_MIC3R_MASK | APW_MIC4R_MASK);
		} else {
			/* Mic4 Auto Power On */
			snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
					APW_MIC4R_MASK, AD_APW_ENABLE << APW_MIC4R_SHIFT);
		}

		/* ANT MDET Un-Masking */
		snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
				T_PDB_ANT_MDET_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_08_IRQ1M,
				ANT_JACKOUT_R_M_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_09_IRQ2M,
				ANT_JACKOUT_F_M_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_D6_DCTR_TEST6,
				T_A2D_ANT_MDET_OUT_MASK, T_A2D_ANT_MDET_NORMAL << T_A2D_ANT_MDET_OUT_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		if (aud3003x->codec_ver == REV_0_1)
			aud3003x_amic_cap_cal(aud3003x, AMIC_R);
		else
			snd_soc_component_update_bits(codec, AUD3003X_2AD_CTRL_ADC3,
					CAP_SWR_MASK, 0x10);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (aud3003x->codec_ver == REV_0_1) {
			i2c_client_change(aud3003x, AUD3003A);

			/* DC offset off */
			snd_soc_component_update_bits(codec, AUD3003X_125_DC_OFFSET_ON,
					DC_OFFSET_RCH_1ST_MASK | DC_OFFSET_RCH_2ND_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		}

		i2c_client_change(aud3003x, AUD3003D);

#ifdef CONFIG_SND_SOC_AUD3003X_VJACK
		/* MicBias HQ mode off */
		snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
				T_CTRV_MCB2_NS_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_C3_ACTR_MCB1,
				CTRV_MCB2_HQ_MASK, 0);
#endif

		/* ANT MDET Masking */
		snd_soc_component_update_bits(codec, AUD3003X_D6_DCTR_TEST6,
				T_A2D_ANT_MDET_OUT_MASK, T_A2D_ANT_MDET_LOW << T_A2D_ANT_MDET_OUT_SHIFT);
		snd_soc_component_update_bits(codec, AUD3003X_08_IRQ1M,
				ANT_JACKOUT_R_M_MASK, ANT_JACKOUT_R_M_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_09_IRQ2M,
				ANT_JACKOUT_F_M_MASK, ANT_JACKOUT_F_M_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
				T_PDB_ANT_MDET_MASK, T_PDB_ANT_MDET_MASK);

		/* Mic4 Auto Power Off */
		snd_soc_component_update_bits(codec, AUD3003X_18_PWAUTO_AD,
				APW_MIC4R_MASK, AD_APW_DISABLE << APW_MIC4R_SHIFT);

		/* Reset off ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		if (play_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003A);

		/* LPF Off */
		snd_soc_component_update_bits(codec, AUD3003X_114_BST4,
				CTMF_LPF_MASK, CTMF_LPF_OFF << CTMF_LPF_SHIFT);

		/* MIC34 selection default set */
		snd_soc_component_update_bits(codec, AUD3003X_11D_BST_OP,
				SEL_MIC34_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int dmic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic_on, play_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	play_on = chop_val & (HP_ON_MASK | EP_ON_MASK);
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(mic_on & DMIC1_ON_MASK)) {
		dev_dbg(codec->dev, "%s: DMIC1 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* DMIC1 Enable */
		snd_soc_component_update_bits(codec, AUD3003X_33_ADC4,
				DMIC_EN1_MASK, DMIC_EN1_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset off ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		if (play_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* DMIC1 Disable */
		snd_soc_component_update_bits(codec, AUD3003X_33_ADC4, DMIC_EN1_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int dmic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic_on, play_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	play_on = chop_val & (HP_ON_MASK | EP_ON_MASK);
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(mic_on & DMIC2_ON_MASK)) {
		dev_dbg(codec->dev, "%s: DMIC2 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* DMIC2 Enable */
		snd_soc_component_update_bits(codec, AUD3003X_33_ADC4,
				DMIC_EN2_MASK, DMIC_EN2_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset off ADC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		if (play_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* DMIC2 Disable */
		snd_soc_component_update_bits(codec, AUD3003X_33_ADC4, DMIC_EN2_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int adc_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, dmic_on, amic_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	dmic_on = chop_val & DMIC_ON_MASK;
	amic_on = chop_val & AMIC_ON_MASK;

	dev_dbg(codec->dev, "%s called, event = %d, dmic = 0x%02x, amic = 0x%02x\n",
			__func__, event, dmic_on, amic_on);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Disable ADC digital mute after configuring ADC */
		cancel_delayed_work(&aud3003x->adc_mute_work);
		if (amic_on)
			queue_delayed_work(aud3003x->adc_mute_wq, &aud3003x->adc_mute_work,
					msecs_to_jiffies(250));
		else if (dmic_on)
			queue_delayed_work(aud3003x->adc_mute_wq, &aud3003x->adc_mute_work,
					msecs_to_jiffies(120));
		else
			queue_delayed_work(aud3003x->adc_mute_wq, &aud3003x->adc_mute_work,
					msecs_to_jiffies(220));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Enable ADC digital mute before configuring ADC */
		aud3003x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

/*
 * DAC(Rx) functions
 */
/*
 * aud3003x_dac_soft_mute() - Set DAC soft mute
 *
 * @codec: SoC audio codec device
 * @channel: Soft mute control for DAC channel
 * @on: dac mute is true, dac unmute is false
 *
 * Desc: When DAC path turn on, analog block noise can be played.
 * For remove this, DAC path was muted always except that it was used.
 */
static void aud3003x_dac_soft_mute(struct snd_soc_component *codec,
		unsigned int channel, bool on)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called, %s\n", __func__, on ? "Mute" : "Unmute");

	if (channel & DA_SMUTEL_MASK)
		mutex_lock(&aud3003x->dacl_mute_lock);
	if (channel & DA_SMUTER_MASK)
		mutex_lock(&aud3003x->dacr_mute_lock);

	i2c_client_change(aud3003x, AUD3003D);
	if (on)
		snd_soc_component_update_bits(codec, AUD3003X_40_PLAY_MODE, channel, channel);
	else
		snd_soc_component_update_bits(codec, AUD3003X_40_PLAY_MODE, channel, 0);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	if (channel & DA_SMUTEL_MASK)
		mutex_unlock(&aud3003x->dacl_mute_lock);
	if (channel & DA_SMUTER_MASK)
		mutex_unlock(&aud3003x->dacr_mute_lock);

	dev_dbg(codec->dev, "%s: work done.\n", __func__);
}

static int dac_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, chop = 0x%02x, event = %d\n",
			__func__, chop_val, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Clock Gate */
		snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
				OVP_CLK_GATE_MASK | SEQ_CLK_GATE_MASK |
				AVC_CLK_GATE_MASK |	DAC_CLK_GATE_MASK,
				CLKGATE0_ENABLE << OVP_CLK_GATE_SHIFT |
				CLKGATE0_ENABLE << SEQ_CLK_GATE_SHIFT |
				CLKGATE0_ENABLE << AVC_CLK_GATE_SHIFT |
				CLKGATE0_ENABLE << DAC_CLK_GATE_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003O);

		/* DAC Trim */
		snd_soc_component_write(codec, AUD3003X_2BC_CTRL_IDAC1_OTP, 0x00);
		snd_soc_component_write(codec, AUD3003X_2BD_CTRL_IDAC2_OTP, 0x17);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* AVC Control */
		snd_soc_component_update_bits(codec, AUD3003X_50_AVC1,
				AVC_VA_EN_MASK, AVC_VA_EN_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* AVC Control disable */
		snd_soc_component_update_bits(codec, AUD3003X_50_AVC1, AVC_VA_EN_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003O);

		/* DAC Trim clear */
		snd_soc_component_write(codec, AUD3003X_2BC_CTRL_IDAC1_OTP, 0x30);
		snd_soc_component_write(codec, AUD3003X_2BD_CTRL_IDAC2_OTP, 0x01);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003D);

		/* Clock Gate disable */
		snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
				OVP_CLK_GATE_MASK | AVC_CLK_GATE_MASK | DAC_CLK_GATE_MASK, 0);
		if (!mic_on)
			snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int epdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, hp_on, ep_on, mic_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	hp_on = chop_val & HP_ON_MASK;
	ep_on = chop_val & EP_ON_MASK;
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, chop = 0x%02x, event = %d\n",
			__func__, chop_val, event);

	if (!ep_on) {
		dev_dbg(codec->dev, "%s: EP is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003O);

		/* DAC Trim */
		snd_soc_component_write(codec, AUD3003X_2BB_CTRL_IDAC0_OTP, 0x21);

		/* DSM Offset Coarse Tuning */
		snd_soc_component_write(codec, AUD3003X_2B0_DSM_OFFSETL,
				snd_soc_component_read32(codec, AUD3003x_2BE_EP_DSM_OFFSETL));
		snd_soc_component_write(codec, AUD3003X_2B1_DSM_OFFSETR,
				snd_soc_component_read32(codec, AUD3003X_2BF_EP_DSM_OFFSETR));

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003A);

		/* CP1 Freq. control according to signal level */
		snd_soc_component_write(codec, AUD3003X_153_DA_CP0, 0x43);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003D);

		/* Clock Gate off for EP */
		snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0, OVP_CLK_GATE_MASK, 0);

		/* EP Mode On */
		snd_soc_component_update_bits(codec, AUD3003X_44_PLAY_MIX0,
				EP_MODE_MASK, EP_MODE_ENABLE << EP_MODE_SHIFT);

		/* AVC Control */
		snd_soc_component_update_bits(codec, AUD3003X_53_AVC4,
				DRC_SLOPE_SEL_MASK, DRC_SLOPE_SEL_5 << DRC_SLOPE_SEL_SHIFT);

		/* AVC Control */
		snd_soc_component_update_bits(codec, AUD3003X_5C_AVC13,
				AMUTE_MASKB_MASK | AVC_HMODE_MASK,
				AMUTE_MASKB_MASK | AVC_HMODE_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Reset DAC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK);

		/* Auto Power on EP */
		snd_soc_component_update_bits(codec, AUD3003X_19_PWAUTO_DA, APW_RCV_MASK,
				DA_APW_ENABLE << APW_RCV_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		/* DAC mute disable */
		aud3003x_dac_soft_mute(codec, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DAC mute enable */
		aud3003x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

		i2c_client_change(aud3003x, AUD3003D);

		/* Auto Power off EP */
		snd_soc_component_update_bits(codec, AUD3003X_19_PWAUTO_DA, APW_RCV_MASK,
				DA_APW_DISABLE << APW_RCV_SHIFT);

		/* Reset off DAC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK, 0);
		if (mic_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* AVC Control clear */
		snd_soc_component_write(codec, AUD3003X_7A_AVC43, 0x70);
		snd_soc_component_write(codec, AUD3003X_5F_AVC16, 0x00);
		snd_soc_component_write(codec, AUD3003X_61_AVC18, 0x00);
		snd_soc_component_write(codec, AUD3003X_63_AVC20, 0x00);

		snd_soc_component_update_bits(codec, AUD3003X_5C_AVC13,
				AMUTE_MASKB_MASK | AVC_HMODE_MASK, 0);

		/* AVC Control clear */
		snd_soc_component_update_bits(codec, AUD3003X_53_AVC4,
				DRC_SLOPE_SEL_MASK, DRC_SLOPE_SEL_0 << DRC_SLOPE_SEL_SHIFT);

		/* EP Mode off */
		snd_soc_component_update_bits(codec, AUD3003X_44_PLAY_MIX0,
				EP_MODE_MASK, EP_MODE_DISABLE << EP_MODE_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003A);

		/* CP1 Freq. control according to signal level clear */
		snd_soc_component_write(codec, AUD3003X_153_DA_CP0, 0x00);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003O);

		/* EP Control clear */
		snd_soc_component_write(codec, AUD3003X_2B7_CTRL_EP0, 0x12);

		/* DAC Trim clear */
		snd_soc_component_write(codec, AUD3003X_2BB_CTRL_IDAC0_OTP, 0x41);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

static int hpdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, hp_on, ep_on, mic_on;

	i2c_client_change(aud3003x, AUD3003D);
	chop_val = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	hp_on = chop_val & HP_ON_MASK;
	ep_on = chop_val & EP_ON_MASK;
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);

	dev_dbg(codec->dev, "%s called, chop = 0x%02x, event = %d\n",
			__func__, chop_val, event);

	if (!hp_on) {
		dev_dbg(codec->dev, "%s: HP is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_client_change(aud3003x, AUD3003O);

		/* DAC Trim */
		snd_soc_component_write(codec, AUD3003X_2BB_CTRL_IDAC0_OTP, 0x01);

		/* DSM Offset Coarse Tuning */
		snd_soc_component_write(codec, AUD3003X_2B0_DSM_OFFSETL,
				snd_soc_component_read32(codec, AUD3003X_2AE_HP_DSM_OFFSETL));
		snd_soc_component_write(codec, AUD3003X_2B1_DSM_OFFSETR,
				snd_soc_component_read32(codec, AUD3003X_2AF_HP_DSM_OFFSETR));

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003D);

		/* AVC Control */
		snd_soc_component_update_bits(codec, AUD3003X_53_AVC4,
				DRC_SLOPE_SEL_MASK, DRC_SLOPE_SEL_7 << DRC_SLOPE_SEL_SHIFT);
		snd_soc_component_update_bits(codec, AUD3003X_5C_AVC13,
				DMUTE_MASKB_MASK | AVC_HMODE_MASK,
				DMUTE_MASKB_MASK | AVC_HMODE_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_5C_AVC13,
				AVC_HEADROOM_MASK, 1 << AVC_HEADROOM_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		switch (aud3003x->playback_aifrate) {
		case AUD3003X_SAMPLE_RATE_48KHZ:
			i2c_client_change(aud3003x, AUD3003O);

			/* OTP Control */
			snd_soc_component_write(codec, AUD3003X_2B3_CTRL_CP4, 0x6F);
			snd_soc_component_write(codec, AUD3003X_2B4_CTRL_HP0, 0x63);
			snd_soc_component_write(codec, AUD3003X_2B5_CTRL_HP1, 0x03);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			i2c_client_change(aud3003x, AUD3003A);

			/* HP pop noise control */
			snd_soc_component_update_bits(codec, AUD3003X_13A_POP_HP,
					EN_UHQA_MASK, 0);

			/* CP1 Freq. control according to signal level */
			snd_soc_component_write(codec, AUD3003X_153_DA_CP0, 0xAD);
			snd_soc_component_write(codec, AUD3003X_154_DA_CP1, 0x50);
			snd_soc_component_write(codec, AUD3003X_155_DA_CP2, 0xAD);
			snd_soc_component_write(codec, AUD3003X_156_DA_CP3, 0x11);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		case AUD3003X_SAMPLE_RATE_192KHZ:
		case AUD3003X_SAMPLE_RATE_384KHZ:
			i2c_client_change(aud3003x, AUD3003O);

			/* OTP Control */
			snd_soc_component_write(codec, AUD3003X_2B3_CTRL_CP4, 0x6F);
			snd_soc_component_write(codec, AUD3003X_2B4_CTRL_HP0, 0x44);
			snd_soc_component_write(codec, AUD3003X_2B5_CTRL_HP1, 0x03);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			i2c_client_change(aud3003x, AUD3003A);

			/* HP pop noise control */
			snd_soc_component_update_bits(codec, AUD3003X_13A_POP_HP,
					EN_UHQA_MASK, EN_UHQA_MASK);

			/* CP1 Freq. control according to signal level */
			snd_soc_component_write(codec, AUD3003X_153_DA_CP0, 0xBD);
			snd_soc_component_write(codec, AUD3003X_154_DA_CP1, 0x50);
			snd_soc_component_write(codec, AUD3003X_155_DA_CP2, 0xBD);
			snd_soc_component_write(codec, AUD3003X_156_DA_CP3, 0x22);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		i2c_client_change(aud3003x, AUD3003D);

		/* Ear L/R ch outtie testmode off */
		snd_soc_component_update_bits(codec, AUD3003X_B7_ODSEL8, T_EN_OUTTIEL_MASK, 0);
		snd_soc_component_update_bits(codec, AUD3003X_B8_ODSEL9, T_EN_OUTTIER_MASK, 0);

		/* Reset DAC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003D);

		/* Auto Power on HP */
		snd_soc_component_update_bits(codec, AUD3003X_19_PWAUTO_DA,
				FAST_CHG_DA_MASK | APW_HP_MASK,
				FAST_CHG_SHORTEST << FAST_CHG_DA_SHIFT |
				DA_APW_ENABLE << APW_HP_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003A);

		/* Auto Power On OVP */
		snd_soc_component_update_bits(codec, AUD3003X_143_CTRL_OVP1,
				OVP_APON_MASK, OVP_APON_MASK);

		i2c_client_change(aud3003x, CODEC_CLOSE);

		/* DAC mute disable */
		aud3003x_dac_soft_mute(codec, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DAC mute enable */
		aud3003x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

		i2c_client_change(aud3003x, AUD3003D);

		/* Auto Power off HP */
		snd_soc_component_update_bits(codec, AUD3003X_19_PWAUTO_DA,
				FAST_CHG_DA_MASK | APW_HP_MASK,
				FAST_CHG_DEFAULT << FAST_CHG_DA_SHIFT |
				DA_APW_DISABLE << APW_HP_SHIFT);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003A);

		/* Auto Power Off OVP */
		snd_soc_component_update_bits(codec, AUD3003X_143_CTRL_OVP1, OVP_APON_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003D);

		/*  Ear L/R ch outtie testmode on */
		snd_soc_component_update_bits(codec, AUD3003X_B7_ODSEL8,
				T_EN_OUTTIEL_MASK, T_EN_OUTTIEL_MASK);
		snd_soc_component_update_bits(codec, AUD3003X_B8_ODSEL9,
				T_EN_OUTTIER_MASK, T_EN_OUTTIER_MASK);

		/* Reset off DAC path */
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK, 0);
		if (mic_on == 0)
			snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,	CORE_RESETB_MASK, 0);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_client_change(aud3003x, AUD3003D);

		/* AVC Control clear */
		snd_soc_component_update_bits(codec, AUD3003X_53_AVC4,
				DRC_SLOPE_SEL_MASK, DRC_SLOPE_SEL_0 << DRC_SLOPE_SEL_SHIFT);
		snd_soc_component_update_bits(codec, AUD3003X_5C_AVC13,
				DMUTE_MASKB_MASK | AVC_HEADROOM_MASK | AVC_HMODE_MASK, 0);

		snd_soc_component_write(codec, AUD3003X_7A_AVC43, 0x70);
		snd_soc_component_write(codec, AUD3003X_5F_AVC16, 0x00);
		snd_soc_component_write(codec, AUD3003X_61_AVC18, 0x00);
		snd_soc_component_write(codec, AUD3003X_63_AVC20, 0x00);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003A);

		/* HP pop noise control off */
		snd_soc_component_update_bits(codec, AUD3003X_13A_POP_HP,
				EN_UHQA_MASK, 0);

		/* CP1 Freq. control according to signal level clear */
		snd_soc_component_write(codec, AUD3003X_153_DA_CP0, 0x00);
		snd_soc_component_write(codec, AUD3003X_154_DA_CP1, 0x46);
		snd_soc_component_write(codec, AUD3003X_155_DA_CP2, 0x00);
		snd_soc_component_write(codec, AUD3003X_156_DA_CP3, 0x00);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		i2c_client_change(aud3003x, AUD3003O);

		/* OTP Control clear */
		snd_soc_component_write(codec, AUD3003X_2B3_CTRL_CP4, 0x6B);
		snd_soc_component_write(codec, AUD3003X_2B4_CTRL_HP0, 0x67);
		snd_soc_component_write(codec, AUD3003X_2B5_CTRL_HP1, 0x23);

		/* DAC Trim clear */
		snd_soc_component_write(codec, AUD3003X_2BB_CTRL_IDAC0_OTP, 0x41);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	}
	return 0;
}

/*
 * dapm widget controls set
 */

/* INP SEL */
static const char * const aud3003x_inp_sel_src_l[] = {
	"AMIC_L ADC_L", "AMIC_R ADC_L", "AMIC_C ADC_L", "Zero ADC_L",
	"DMIC1L ADC_L", "DMIC1R ADC_L", "DMIC2L ADC_L", "DMIC2R ADC_L"
};
static const char * const aud3003x_inp_sel_src_r[] = {
	"AMIC_R ADC_R", "AMIC_L ADC_R", "AMIC_C ADC_R", "Zero ADC_R",
	"DMIC1L ADC_R", "DMIC1R ADC_R", "DMIC2L ADC_R", "DMIC2R ADC_R"
};
static const char * const aud3003x_inp_sel_src_c[] = {
	"AMIC_C ADC_C", "AMIC_L ADC_C", "AMIC_R ADC_C", "Zero ADC_C",
	"DMIC1L ADC_C", "DMIC1R ADC_C", "DMIC2L ADC_C", "DMIC2R ADC_C"
};

static SOC_ENUM_SINGLE_DECL(aud3003x_inp_sel_enum_l, AUD3003X_31_ADC2,
		INP_SEL_L_SHIFT, aud3003x_inp_sel_src_l);
static SOC_ENUM_SINGLE_DECL(aud3003x_inp_sel_enum_r, AUD3003X_31_ADC2,
		INP_SEL_R_SHIFT, aud3003x_inp_sel_src_r);
static SOC_ENUM_SINGLE_DECL(aud3003x_inp_sel_enum_c, AUD3003X_32_ADC3,
		INP_SEL_C_SHIFT, aud3003x_inp_sel_src_c);

static const struct snd_kcontrol_new aud3003x_inp_sel_l =
		SOC_DAPM_ENUM("INP_SEL_L", aud3003x_inp_sel_enum_l);
static const struct snd_kcontrol_new aud3003x_inp_sel_r =
		SOC_DAPM_ENUM("INP_SEL_R", aud3003x_inp_sel_enum_r);
static const struct snd_kcontrol_new aud3003x_inp_sel_c =
		SOC_DAPM_ENUM("INP_SEL_C", aud3003x_inp_sel_enum_c);

/* MIC On */
static const struct snd_kcontrol_new mic1_on[] = {
	SOC_DAPM_SINGLE("MIC1 On", AUD3003X_1E_CHOP1, 0, 1, 0),
};
static const struct snd_kcontrol_new mic2_on[] = {
	SOC_DAPM_SINGLE("MIC2 On", AUD3003X_1E_CHOP1, 1, 1, 0),
};
static const struct snd_kcontrol_new mic3_on[] = {
	SOC_DAPM_SINGLE("MIC3 On", AUD3003X_1E_CHOP1, 2, 1, 0),
};
static const struct snd_kcontrol_new mic4_on[] = {
	SOC_DAPM_SINGLE("MIC4 On", AUD3003X_1E_CHOP1, 3, 1, 0),
};

/* DMIC On */
static const struct snd_kcontrol_new dmic1_on[] = {
	SOC_DAPM_SINGLE("DMIC1 On", AUD3003X_1E_CHOP1, 4, 1, 0),
};
static const struct snd_kcontrol_new dmic2_on[] = {
	SOC_DAPM_SINGLE("DMIC2 On", AUD3003X_1E_CHOP1, 5, 1, 0),
};

/* Rx Devices */
static const struct snd_kcontrol_new ep_on[] = {
	SOC_DAPM_SINGLE("EP On", AUD3003X_1E_CHOP1, 6, 1, 0),
};
static const struct snd_kcontrol_new hp_on[] = {
	SOC_DAPM_SINGLE("HP On", AUD3003X_1E_CHOP1, 7, 1, 0),
};

static const struct snd_soc_dapm_widget aud3003x_dapm_widgets[] = {
	/*
	 * ADC(Tx) dapm widget
	 */
	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("IN3L"),
	SND_SOC_DAPM_INPUT("IN4L"),

	SND_SOC_DAPM_SUPPLY("VMID", SND_SOC_NOPM, 0, 0, vmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DVMID", SND_SOC_NOPM, 0, 0, dvmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("MIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC3_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic3_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC4_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic4_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("DMIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("MIC1", SND_SOC_NOPM, 0, 0, mic1_on),
	SND_SOC_DAPM_SWITCH("MIC2", SND_SOC_NOPM, 0, 0, mic2_on),
	SND_SOC_DAPM_SWITCH("MIC3", SND_SOC_NOPM, 0, 0, mic3_on),
	SND_SOC_DAPM_SWITCH("MIC4", SND_SOC_NOPM, 0, 0, mic4_on),

	SND_SOC_DAPM_SWITCH("DMIC1", SND_SOC_NOPM, 0, 0, dmic1_on),
	SND_SOC_DAPM_SWITCH("DMIC2", SND_SOC_NOPM, 0, 0, dmic2_on),

	SND_SOC_DAPM_MUX("INP_SEL_L", SND_SOC_NOPM, 0, 0, &aud3003x_inp_sel_l),
	SND_SOC_DAPM_MUX("INP_SEL_R", SND_SOC_NOPM, 0, 0, &aud3003x_inp_sel_r),
	SND_SOC_DAPM_MUX("INP_SEL_C", SND_SOC_NOPM, 0, 0, &aud3003x_inp_sel_c),

	SND_SOC_DAPM_ADC_E("ADC", "AIF Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "AIF2 Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/*
	 * DAC(Rx) dapm widget
	 */
	SND_SOC_DAPM_SWITCH("EP", SND_SOC_NOPM, 0, 0, ep_on),
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM, 0, 0, hp_on),

	SND_SOC_DAPM_OUT_DRV_E("EPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, epdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, hpdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC", "AIF2 Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIC("DMIC_BIAS", aud3003x_digital_micbias),
	SND_SOC_DAPM_OUTPUT("EPOUTN"),
	SND_SOC_DAPM_OUTPUT("HPOUTLN"),
};

static const struct snd_soc_dapm_route aud3003x_dapm_routes[] = {
	/*
	 * ADC(Tx) dapm route
	 */
	{"MIC1_PGA", NULL, "IN1L"},
	{"MIC1_PGA", NULL, "VMID"},
	{"MIC1", "MIC1 On", "MIC1_PGA"},

	{"MIC2_PGA", NULL, "IN2L"},
	{"MIC2_PGA", NULL, "VMID"},
	{"MIC2", "MIC2 On", "MIC2_PGA"},

	{"MIC3_PGA", NULL, "IN3L"},
	{"MIC3_PGA", NULL, "VMID"},
	{"MIC3", "MIC3 On", "MIC3_PGA"},

	{"MIC4_PGA", NULL, "IN4L"},
	{"MIC4_PGA", NULL, "VMID"},
	{"MIC4", "MIC4 On", "MIC4_PGA"},

	{"DMIC1_PGA", NULL, "IN1L"},
	{"DMIC1_PGA", NULL, "DVMID"},
	{"DMIC1", "DMIC1 On", "DMIC1_PGA"},

	{"DMIC2_PGA", NULL, "IN2L"},
	{"DMIC2_PGA", NULL, "DVMID"},
	{"DMIC2", "DMIC2 On", "DMIC2_PGA"},

	{"INP_SEL_L", "AMIC_L ADC_L", "MIC1"},
	{"INP_SEL_L", "AMIC_C ADC_L", "MIC2"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC3"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC4"},
	{"INP_SEL_L", "DMIC1L ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC1R ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC2L ADC_L", "DMIC2"},
	{"INP_SEL_L", "DMIC2R ADC_L", "DMIC2"},

	{"INP_SEL_R", "AMIC_L ADC_R", "MIC1"},
	{"INP_SEL_R", "AMIC_C ADC_R", "MIC2"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC3"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC4"},
	{"INP_SEL_R", "DMIC1L ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC1R ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC2L ADC_R", "DMIC2"},
	{"INP_SEL_R", "DMIC2R ADC_R", "DMIC2"},

	{"INP_SEL_C", "AMIC_L ADC_C", "MIC1"},
	{"INP_SEL_C", "AMIC_C ADC_C", "MIC2"},
	{"INP_SEL_C", "AMIC_R ADC_C", "MIC3"},
	{"INP_SEL_C", "AMIC_R ADC_C", "MIC4"},
	{"INP_SEL_C", "DMIC1L ADC_C", "DMIC1"},
	{"INP_SEL_C", "DMIC1R ADC_C", "DMIC1"},
	{"INP_SEL_C", "DMIC2L ADC_C", "DMIC2"},
	{"INP_SEL_C", "DMIC2R ADC_C", "DMIC2"},

	{"ADC", NULL, "INP_SEL_L"},
	{"ADC", NULL, "INP_SEL_R"},
	{"ADC", NULL, "INP_SEL_C"},

	{"AIF Capture", NULL, "ADC"},
	{"AIF2 Capture", NULL, "ADC"},

	/*
	 * DAC(Rx) dapm route
	 */
	{"DAC", NULL, "AIF Playback"},
	{"DAC", NULL, "AIF2 Playback"},

	{"EPDRV", NULL, "DAC"},
	{"EP", "EP On", "EPDRV"},
	{"EPOUTN", NULL, "EP"},

	{"HPDRV", NULL, "DAC"},
	{"HP", "HP On", "HPDRV"},
	{"HPOUTLN", NULL, "HP"},
};

static int aud3003x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *codec = dai->component;

	dev_dbg(codec->dev, "%s called. fmt: %d\n", __func__, fmt);

	return 0;
}

static int aud3003x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	i2c_client_change(aud3003x, AUD3003D);
	/* Internal Clock On */
	snd_soc_component_update_bits(codec, AUD3003X_10_CLKGATE0,
			COM_CLK_GATE_MASK, COM_CLK_GATE_MASK);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	return 0;
}

/*
 * capture_hw_params() - Register setting for capture
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before capture.
 */
static void capture_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, aud3003x->capture_aifrate, cur_aifrate);
	aud3003x->capture_on = true;
	aud3003x_adc_digital_mute(codec, ADC_MUTE_ALL, true);

	if (aud3003x->capture_aifrate != cur_aifrate) {
		switch (cur_aifrate) {
		case AUD3003X_SAMPLE_RATE_48KHZ:
			i2c_client_change(aud3003x, AUD3003D);

			/* UHQA I/O strength selection */
			snd_soc_component_update_bits(codec, AUD3003X_2F_DMIC_ST,
					DMIC_ST_MASK, DMIC_IO_ST_1);

			/* 48K output format selection */
			snd_soc_component_update_bits(codec, AUD3003X_29_IF_FORM6,
					ADC_OUT_FORMAT_SEL_MASK, ADC_FM_48K_AT_48K);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		case AUD3003X_SAMPLE_RATE_192KHZ:
			i2c_client_change(aud3003x, AUD3003D);

			/* UHQA I/O strength selection */
			snd_soc_component_update_bits(codec, AUD3003X_2F_DMIC_ST,
					DMIC_ST_MASK, DMIC_IO_ST_2);

			/* 192K output format selection */
			snd_soc_component_update_bits(codec, AUD3003X_29_IF_FORM6,
					ADC_OUT_FORMAT_SEL_MASK, ADC_FM_192K_AT_192K);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		case AUD3003X_SAMPLE_RATE_384KHZ:
			i2c_client_change(aud3003x, AUD3003D);

			/* UHQA I/O strength selection */
			snd_soc_component_update_bits(codec, AUD3003X_2F_DMIC_ST,
					DMIC_ST_MASK, DMIC_IO_ST_2);

			/* 384K output format selection */
			snd_soc_component_update_bits(codec, AUD3003X_29_IF_FORM6,
					ADC_OUT_FORMAT_SEL_MASK, ADC_FM_192K_ZP_AT_384K);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		default:
			dev_err(codec->dev, "%s: sample rate error!\n", __func__);
			break;
		}
		aud3003x->capture_aifrate = cur_aifrate;
	}
}

/*
 * playback_hw_params() - Register setting for playback
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before playback.
 */
static void playback_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, aud3003x->playback_aifrate, cur_aifrate);
	aud3003x->playback_on = true;

	if (aud3003x->playback_aifrate != cur_aifrate) {
		switch (cur_aifrate) {
		case AUD3003X_SAMPLE_RATE_48KHZ:
			i2c_client_change(aud3003x, AUD3003D);

			/* DSM Mode Selection */
			snd_soc_component_write(codec, AUD3003X_93_DSM_CON1, 0x52);

			/* DAC Clock Mode Selection */
			snd_soc_component_write(codec, AUD3003X_16_CLK_MODE_SEL, 0x04);

			/* DSM Rate Selection */
			snd_soc_component_write(codec, AUD3003X_40_PLAY_MODE, 0x04);

			/* DAC gain trim */
			snd_soc_component_write(codec, AUD3003X_47_TRIM_DAC0, 0xF7);
			snd_soc_component_write(codec, AUD3003X_48_TRIM_DAC1, 0x4F);

			/* AVC delay default setting */
			snd_soc_component_write(codec, AUD3003X_71_AVC34, 0x18);
			snd_soc_component_write(codec, AUD3003X_72_AVC35, 0xDD);
			snd_soc_component_write(codec, AUD3003X_73_AVC36, 0x17);
			snd_soc_component_write(codec, AUD3003X_74_AVC37, 0xE9);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		case AUD3003X_SAMPLE_RATE_192KHZ:
			i2c_client_change(aud3003x, AUD3003D);

			/* DSM Mode Selection */
			snd_soc_component_write(codec, AUD3003X_93_DSM_CON1, 0x95);

			/* DAC Clock Mode Selection */
			snd_soc_component_write(codec, AUD3003X_16_CLK_MODE_SEL, 0x06);

			/* DSM Rate Selection */
			snd_soc_component_write(codec, AUD3003X_40_PLAY_MODE, 0x44);

			/* DAC gain trim */
			snd_soc_component_write(codec, AUD3003X_47_TRIM_DAC0, 0xFF);
			snd_soc_component_write(codec, AUD3003X_48_TRIM_DAC1, 0x1E);

			/* AVC delay default setting */
			snd_soc_component_write(codec, AUD3003X_71_AVC34, 0xC7);
			snd_soc_component_write(codec, AUD3003X_72_AVC35, 0x39);
			snd_soc_component_write(codec, AUD3003X_73_AVC36, 0xC4);
			snd_soc_component_write(codec, AUD3003X_74_AVC37, 0x39);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		case AUD3003X_SAMPLE_RATE_384KHZ:
			i2c_client_change(aud3003x, AUD3003D);

			/* DSM Mode Selection */
			snd_soc_component_write(codec, AUD3003X_93_DSM_CON1, 0x95);

			/* DAC Clock Mode Selection */
			snd_soc_component_write(codec, AUD3003X_16_CLK_MODE_SEL, 0x07);

			/* DSM Rate Selection */
			snd_soc_component_write(codec, AUD3003X_40_PLAY_MODE, 0x54);

			/* DAC gain trim */
			snd_soc_component_write(codec, AUD3003X_47_TRIM_DAC0, 0xFD);
			snd_soc_component_write(codec, AUD3003X_48_TRIM_DAC1, 0x12);

			/* AVC delay default setting */
			snd_soc_component_write(codec, AUD3003X_71_AVC34, 0xC7);
			snd_soc_component_write(codec, AUD3003X_72_AVC35, 0x3A);
			snd_soc_component_write(codec, AUD3003X_73_AVC36, 0xC4);
			snd_soc_component_write(codec, AUD3003X_74_AVC37, 0x3A);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			break;
		default:
			dev_err(codec->dev, "%s: sample rate error!\n", __func__);
			break;
		}
		aud3003x->playback_aifrate = cur_aifrate;
	}
}

static int aud3003x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	unsigned int cur_aifrate, width, channels;

	/* Get params */
	cur_aifrate = params_rate(params);
	width = params_width(params);
	channels = params_channels(params);

	dev_dbg(codec->dev, "(%s) %s called. aifrate: %d, width: %d, channels: %d\n",
			substream->stream ? "C" : "P", __func__, cur_aifrate, width, channels);

	switch (width) {
	case BIT_RATE_16:
		i2c_client_change(aud3003x, AUD3003D);

		/* I2S 16bit/32fs Set */
		snd_soc_component_update_bits(codec, AUD3003X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_16);
		snd_soc_component_update_bits(codec, AUD3003X_22_IF_FORM3,
				I2S_XFS_MASK, I2S_XFS_32);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	case BIT_RATE_32:
		i2c_client_change(aud3003x, AUD3003D);

		/* I2S 32bit/64fs Set */
		snd_soc_component_update_bits(codec, AUD3003X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_32);
		snd_soc_component_update_bits(codec, AUD3003X_22_IF_FORM3,
				I2S_XFS_MASK, I2S_XFS_64);

		i2c_client_change(aud3003x, CODEC_CLOSE);
		break;
	default:
		dev_err(codec->dev, "%s: bit rate error!\n", __func__);
		break;
	}

	if (substream->stream)
		capture_hw_params(codec, cur_aifrate);
	else
		playback_hw_params(codec, cur_aifrate);

	return 0;
}

static void aud3003x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	if (substream->stream)
		aud3003x->capture_on = false;
	else
		aud3003x->playback_on = false;

	if (!aud3003x->capture_on && !aud3003x->playback_on) {
		i2c_client_change(aud3003x, AUD3003D);
		snd_soc_component_update_bits(codec, AUD3003X_14_RESETB0,
				CORE_RESETB_MASK, 0);
		i2c_client_change(aud3003x, CODEC_CLOSE);
	}
}

static const struct snd_soc_dai_ops aud3003x_dai_ops = {
	.set_fmt = aud3003x_dai_set_fmt,
	.startup = aud3003x_dai_startup,
	.hw_params = aud3003x_dai_hw_params,
	.shutdown = aud3003x_dai_shutdown,
};

#define AUD3003X_RATES		SNDRV_PCM_RATE_8000_192000
#define AUD3003X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
							SNDRV_PCM_FMTBIT_S20_3LE | \
							SNDRV_PCM_FMTBIT_S24_LE  | \
							SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aud3003x_dai[] = {
	{
		.name = "aud3003x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = AUD3003X_RATES,
			.formats = AUD3003X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = AUD3003X_RATES,
			.formats = AUD3003X_FORMATS,
		},
		.ops = &aud3003x_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "aud3003x-aif2",
		.id = 2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AUD3003X_RATES,
			.formats = AUD3003X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AUD3003X_RATES,
			.formats = AUD3003X_FORMATS,
		},
		.ops = &aud3003x_dai_ops,
		.symmetric_rates = 1,
	},
};

void aud3003x_reset_io_selector_bits(struct snd_soc_component *codec)
{
	/* Reset output selector bits */
	snd_soc_component_update_bits(codec, AUD3003X_1E_CHOP1,
			HP_ON_MASK | EP_ON_MASK, 0x0);
}

static void aud3003x_regmap_sync(struct device *dev)
{
	struct aud3003x_priv *aud3003x = dev_get_drvdata(dev);
	struct snd_soc_component *codec = aud3003x->codec;
	unsigned char reg[AUD3003X_REGCACHE_SYNC_END] = {0,};
	int p_reg, p_map;

	for (p_map = AUD3003D; p_map <= AUD3003O; p_map++) {
		i2c_client_change(aud3003x, p_map);

		/* Read from Cache */
		for (p_reg = 0; p_reg < AUD3003X_REGCACHE_SYNC_END; p_reg++) {
			if (read_from_cache(dev, p_reg, p_map))
				reg[p_reg] = (unsigned char) snd_soc_component_read32(codec, p_reg);
		}

		regcache_cache_bypass(aud3003x->regmap[p_map], true);
		/* Update HW */
		for (p_reg = 0; p_reg < AUD3003X_REGCACHE_SYNC_END; p_reg++)
			if (write_to_hw(dev, p_reg, p_map))
				snd_soc_component_write(codec, p_reg, reg[p_reg]);
		regcache_cache_bypass(aud3003x->regmap[p_map], false);

		i2c_client_change(aud3003x, CODEC_CLOSE);
	}
}

static void aud3003x_reg_restore(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	i2c_client_change(aud3003x, AUD3003D);

	snd_soc_component_update_bits(codec, AUD3003X_D0_DCTR_CM,
			PDB_JD_CLK_EN_MASK, PDB_JD_CLK_EN_MASK);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	msleep(40);

	if (!aud3003x->is_probe_done) {
		aud3003x_regmap_sync(codec->dev);
		aud3003x_reset_io_selector_bits(codec);
	} else {
		aud3003x_regmap_sync(codec->dev);
	}
}

int aud3003x_regulators_enable(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	int ret;

	ret = regulator_enable(aud3003x->vdd);
	ret = regulator_enable(aud3003x->vdd2);

	return ret;
}

void aud3003x_regulators_disable(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	if (aud3003x->regulator_count == 0) {
		regulator_disable(aud3003x->vdd);
		regulator_disable(aud3003x->vdd2);
	}
}

int aud3003x_enable(struct device *dev)
{
	struct aud3003x_priv *aud3003x = dev_get_drvdata(dev);
	int p_map;

	dev_dbg(dev, "(*) %s\n", __func__);
	aud3003x->is_suspend = false;
	aud3003x->regulator_count++;

	//abox_enable_mclk(true);
	aud3003x_acpm_update_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F59_ETC_OTP3,
			EN_OSC_32K_MASK, EN_OSC_32K_MASK);

	aud3003x_regulators_enable(aud3003x->codec);

	/* Disable cache_only feature and sync the cache with h/w */
	for (p_map = AUD3003D; p_map <= AUD3003O; p_map++)
		regcache_cache_only(aud3003x->regmap[p_map], false);

	aud3003x_reg_restore(aud3003x->codec);

	return 0;
}

int aud3003x_disable(struct device *dev)
{
	struct aud3003x_priv *aud3003x = dev_get_drvdata(dev);
	int p_map;

	dev_dbg(dev, "(*) %s\n", __func__);

	aud3003x->is_suspend = true;
	aud3003x->regulator_count--;

	/* As device is going to suspend-state, limit the writes to cache */
	for (p_map = AUD3003D; p_map <= AUD3003O; p_map++)
		regcache_cache_only(aud3003x->regmap[p_map], true);

	aud3003x_regulators_disable(aud3003x->codec);

	//abox_enable_mclk(false);

	return 0;
}

static int aud3003x_sys_resume(struct device *dev)
{
#ifndef CONFIG_PM
	struct aud3003x_priv *aud3003x = dev_get_drvdata(dev);

	if (!aud3003x->is_suspend) {
		dev_dbg(dev, "(*)aud3003x not resuming, cp functioning\n");
		return 0;
	}
	dev_dbg(dev, "(*) %s\n", __func__);
	aud3003x->pm_suspend = false;
	aud3003x_enable(dev);
#endif

	return 0;
}

static int aud3003x_sys_suspend(struct device *dev)
{
#ifndef CONFIG_PM
	struct aud3003x_priv *aud3003x = dev_get_drvdata(dev);

	if (abox_is_on()) {
		dev_dbg(dev, "(*)Don't suspend aud3003x, cp functioning\n");
		return 0;
	}
	dev_dbg(dev, "(*) %s\n", __func__);
	aud3003x->pm_suspend = true;
	aud3003x_disable(dev);
#endif

	return 0;
}

#ifdef CONFIG_PM
static int aud3003x_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "(*) %s\n", __func__);
	aud3003x_enable(dev);

	return 0;
}

static int aud3003x_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "(*) %s\n", __func__);
	aud3003x_disable(dev);

	return 0;
}
#endif

static const struct dev_pm_ops aud3003x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			aud3003x_sys_suspend,
			aud3003x_sys_resume)
#ifdef CONFIG_PM
	SET_RUNTIME_PM_OPS(
			aud3003x_runtime_suspend,
			aud3003x_runtime_resume,
			NULL)
#endif
};

static void aud3003x_i2c_parse_dt(struct aud3003x_priv *aud3003x)
{
	struct device *dev = aud3003x->dev;
	struct device_node *np = dev->of_node;
	unsigned int feature_flag;
	int ret;

	/* model feature flag */
	ret = of_property_read_u32(dev->of_node, "use-feature-flag", &feature_flag);
	if (!ret)
		aud3003x->model_feature_flag = feature_flag;
	else
		aud3003x->model_feature_flag = 0;

	aud3003x->dmic_bias_gpio = of_get_named_gpio(np, "dmic-bias-gpio", 0);
	if (aud3003x->dmic_bias_gpio < 0)
		dev_err(dev, "%s: cannot find dmic bias gpio in the dt\n", __func__);
	else
		dev_dbg(dev, "%s: dmic bias gpio = %d\n",
				__func__, aud3003x->dmic_bias_gpio);

	dev_dbg(dev, "Codec Feature Flag: 0x%02x\n", feature_flag);
}

/*
 * aud3003x_register_initialize() - Codec reigster initialize
 *
 * When system boot, codec should successful probe with specific parameters.
 * These specific values change the chip default register value
 * to arbitrary value in booting in order to modify an issue
 * that cannot be predicted during the design.
 *
 * The values provided in this function are hard-coded register values, and we
 * need not update these values as per bit-fields.
 */
static void aud3003x_register_initialize(void *context)
{
	struct snd_soc_component *codec = (struct snd_soc_component *)context;
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

#ifdef CONFIG_PM
	pm_runtime_get_sync(codec->dev);
#else
	aud3003x_enable(codec->dev);
#endif

	dev_dbg(codec->dev, "%s called, setting defaults\n", __func__);

	i2c_client_change(aud3003x, AUD3003D);

	/* VTS HighZ Setting */
	snd_soc_component_update_bits(codec, AUD3003X_2F_DMIC_ST,
			VTS_DATA_ENB_MASK, VTS_DATA_ENB_MASK);

	/* Clock Gate */
	snd_soc_component_write(codec, AUD3003X_11_CLKGATE1, 0x66);

	/* BGR Skip Mode */
	snd_soc_component_write(codec, AUD3003X_18_PWAUTO_AD, 0x20);

	/* DVS Mode Selection */
	snd_soc_component_write(codec, AUD3003X_1B_DVS_CTRL, 0x85);

	/* DAC Gain Trim */
	snd_soc_component_write(codec, AUD3003X_46_PLAY_MIX2, 0x00);

	/* Offset range control */
	snd_soc_component_write(codec, AUD3003X_4B_OFFSET_CTR, 0xF0);

	/* HP Cross Talk, AVC initial */
	snd_soc_component_write(codec, AUD3003X_68_AVC25, 0x08);
	snd_soc_component_write(codec, AUD3003X_9D_AVC_DWA_OFF_THRES, 0x00);
	snd_soc_component_write(codec, AUD3003X_9E_AVC_BTS_ON_THRES, 0xFF);

	/* AVC Control initial */
	snd_soc_component_write(codec, AUD3003X_7A_AVC43, 0x70);

	/* PDB_BGR Testmode on */
	snd_soc_component_update_bits(codec, AUD3003X_B0_ODSEL1,
			T_PDB_BGR_MASK | T_EN_BGR_PULLDN_MASK,
			T_PDB_BGR_MASK | T_EN_BGR_PULLDN_MASK);

	/* Testmode EN LCP 1/2 */
	snd_soc_component_write(codec, AUD3003X_B3_ODSEL4, 0x04);
	snd_soc_component_write(codec, AUD3003X_B4_ODSEL5, 0x10);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	i2c_client_change(aud3003x, AUD3003A);

	/* PDB_BGR on */
	snd_soc_component_update_bits(codec, AUD3003X_102_REF_CTRL1,
			PDB_BGR_MASK, PDB_BGR_MASK);
	snd_soc_component_update_bits(codec, AUD3003X_102_REF_CTRL1,
			EN_BGR_PULL_DN_MASK, 0);

	/* Control VTS */
	snd_soc_component_write(codec, AUD3003X_10A_CTRL_VTS2, 0x46);

	/* ISI SW on (DSML) */
	snd_soc_component_write(codec, AUD3003X_115_DSM1, 0x10);
	snd_soc_component_write(codec, AUD3003X_117_DSM2, 0x10);
	snd_soc_component_write(codec, AUD3003X_119_DSM3, 0x10);

	/* Analog Mic Boost */
	snd_soc_component_write(codec, AUD3003X_111_BST1, 0x20);
	snd_soc_component_write(codec, AUD3003X_112_BST2, 0x20);
	snd_soc_component_write(codec, AUD3003X_113_BST3, 0x20);
	snd_soc_component_write(codec, AUD3003X_114_BST4, 0x20);

	/* Mic Calibration Skip */
	if (aud3003x->codec_ver == REV_0_0)
		snd_soc_component_update_bits(codec, AUD3003X_115_DSM1,
				CAL_SKIP_MASK, CAL_SKIP_MASK);

	/* EN LCP 1/2 */
	snd_soc_component_write(codec, AUD3003X_11C_BSTC, 0xEC);

	/* DC offset clear */
	snd_soc_component_write(codec, AUD3003X_125_DC_OFFSET_ON, 0x00);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	i2c_client_change(aud3003x, AUD3003O);

	/* Mic Calibration for Rev 0.0 */
	if (aud3003x->codec_ver == REV_0_0) {
		snd_soc_component_update_bits(codec, AUD3003X_2AB_CTRL_ADC1,
				CAP_SWL_MASK, 0x10);
		snd_soc_component_update_bits(codec, AUD3003X_2AC_CTRL_ADC2,
				CAP_SWC_MASK, 0x10);
		snd_soc_component_update_bits(codec, AUD3003X_2AD_CTRL_ADC3,
				CAP_SWR_MASK, 0x10);
	}

	i2c_client_change(aud3003x, CODEC_CLOSE);

	/* ADC/DAC Mute */
	aud3003x_adc_digital_mute(codec, ADC_MUTE_ALL , true);
	aud3003x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

	i2c_client_change(aud3003x, AUD3003D);

	/* PDB_JD_CLK_EN */
	snd_soc_component_update_bits(codec, AUD3003X_D0_DCTR_CM, PDB_JD_CLK_EN_MASK, 0);
	msleep(20);
	snd_soc_component_update_bits(codec, AUD3003X_D0_DCTR_CM,
			PDB_JD_CLK_EN_MASK, PDB_JD_CLK_EN_MASK);

	i2c_client_change(aud3003x, CODEC_CLOSE);

	/* PDB BGR Charging Time */
	msleep(60);

	/* All boot time hardware access is done. Put the device to sleep. */
#ifdef CONFIG_PM
	pm_runtime_put_sync(codec->dev);
#else
	aud3003x_disable(codec->dev);
#endif
}

static int aud3003x_codec_probe(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	pr_err("Codec Digital Driver Probe: (%s)\n", __func__);

	aud3003x->codec = codec;

	/* register codec power */
	aud3003x->vdd = devm_regulator_get(codec->dev, "vdd_aldo1");
	if (IS_ERR(aud3003x->vdd)) {
		dev_warn(codec->dev, "failed to get regulator vdd\n");
		return PTR_ERR(aud3003x->vdd);
	}

	aud3003x->vdd2 = devm_regulator_get(codec->dev, "vdd_aldo2");
	if (IS_ERR(aud3003x->vdd2)) {
		dev_warn(codec->dev, "failed to get regulator vdd2\n");
		return PTR_ERR(aud3003x->vdd2);
	}

	/* initialize codec_priv variable */
	aud3003x_acpm_read_reg(AUD3003X_COMMON_ADDR, 0x04, &aud3003x->codec_ver);
	aud3003x->playback_aifrate = 0;
	aud3003x->capture_aifrate = 0;
	aud3003x->is_probe_done = true;
	aud3003x->mic_status = 0;
	aud3003x->hp_vts_on = false;
	aud3003x->hp_avc_gain = 0;
	aud3003x->rcv_avc_gain = 0;
	aud3003x->playback_on = false;
	aud3003x->capture_on = false;
	aud3003x->amic_cal_done = false;

	/* initialize workqueue */
	INIT_DELAYED_WORK(&aud3003x->adc_mute_work, aud3003x_adc_mute_work);
	aud3003x->adc_mute_wq = create_singlethread_workqueue("adc_mute_wq");
	if (aud3003x->adc_mute_wq == NULL) {
		dev_err(codec->dev, "Failed to create adc_mute_wq\n");
		return -ENOMEM;
	}

	/* initialize mutex lock */
	mutex_init(&aud3003x->regcache_lock);
	mutex_init(&aud3003x->adc_mute_lock);
	mutex_init(&aud3003x->dacl_mute_lock);
	mutex_init(&aud3003x->dacr_mute_lock);
	mutex_init(&aud3003x->regmap_lock);
	mutex_init(&aud3003x->cap_cal_lock);

	msleep(20);

	/* dt parse for codec */
	aud3003x_i2c_parse_dt(aud3003x);
	/* register value init for codec */
	aud3003x_register_initialize(codec);

	/* dmic bias gpio initialize */
	if (aud3003x->dmic_bias_gpio > 0) {
		if (gpio_request(aud3003x->dmic_bias_gpio, "aud3003x_dmic_bias") < 0)
			dev_err(aud3003x->dev, "%s: Request for %d GPIO failed\n",
					__func__, (int)aud3003x->dmic_bias_gpio);
		if (gpio_direction_output(aud3003x->dmic_bias_gpio, 0) < 0)
			dev_err(aud3003x->dev, "%s: GPIO direction to output failed!\n",
					__func__);
	}

	/* Ignore suspend status for DAPM endpoint */
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "HPOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "EPOUTN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN1L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN2L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN3L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN4L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF2 Capture");
	snd_soc_dapm_sync(snd_soc_component_get_dapm(codec));

	dev_dbg(codec->dev, "Codec probe done. Codec Revision: 0.%d\n",
			aud3003x->codec_ver);
#if defined(CONFIG_SND_SOC_AUD3003X_5PIN) || defined(CONFIG_SND_SOC_AUD3003X_6PIN)
	/* Jack probe */
	aud3003x_jack_probe(codec);
#endif
	return 0;
}

static void aud3003x_codec_remove(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	dev_dbg(codec->dev, "(*) %s called\n", __func__);

	destroy_workqueue(aud3003x->adc_mute_wq);

#if defined(CONFIG_SND_SOC_AUD3003X_5PIN) || defined(CONFIG_SND_SOC_AUD3003X_6PIN)
	aud3003x_jack_remove(codec);
#endif
	aud3003x_regulators_disable(codec);
}

static struct snd_soc_component_driver soc_codec_dev_aud3003x = {
	.probe = aud3003x_codec_probe,
	.remove = aud3003x_codec_remove,
	.controls = aud3003x_snd_controls,
	.num_controls = ARRAY_SIZE(aud3003x_snd_controls),
	.dapm_widgets = aud3003x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aud3003x_dapm_widgets),
	.dapm_routes = aud3003x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(aud3003x_dapm_routes),
	.use_pmdown_time = false,
	.idle_bias_on = false,
};

static int aud3003x_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct aud3003x_priv *aud3003x;
	struct device *dev;
	int ret;

	dev_dbg(&i2c->dev, "Codec I2C Probe: (%s) name: %s, i2c addr: 0x%02x\n",
		__func__, id->name, (int) i2c->addr);

	aud3003x = kzalloc(sizeof(struct aud3003x_priv), GFP_KERNEL);
	if (aud3003x == NULL)
		return -ENOMEM;

	aud3003x->dev = &i2c->dev;
	aud3003x->is_probe_done = false;
	aud3003x->regulator_count = 0;

	aud3003x->i2c_priv[AUD3003D] = i2c;
	aud3003x->i2c_priv[AUD3003A] = i2c_new_dummy(i2c->adapter, AUD3003X_ANALOG_ADDR);
	aud3003x->i2c_priv[AUD3003O] = i2c_new_dummy(i2c->adapter, AUD3003X_OTP_ADDR);

	aud3003x->regmap[AUD3003D] =
		devm_regmap_init_i2c(aud3003x->i2c_priv[AUD3003D], &aud3003x_regmap);
	if (IS_ERR(aud3003x->regmap[AUD3003D])) {
		dev_err(&i2c->dev, "Failed to allocate digital regmap: %li\n",
				PTR_ERR(aud3003x->regmap[AUD3003D]));
		ret = -ENOMEM;
		goto err;
	}

	aud3003x->regmap[AUD3003A] =
		devm_regmap_init_i2c(aud3003x->i2c_priv[AUD3003A], &aud3003x_analog_regmap);
	if (IS_ERR(aud3003x->regmap[AUD3003A])) {
		dev_err(&i2c->dev, "Failed to allocate analog regmap: %li\n",
				PTR_ERR(aud3003x->regmap[AUD3003A]));
		ret = -ENOMEM;
		goto err;
	}

	aud3003x->regmap[AUD3003O] =
		devm_regmap_init_i2c(aud3003x->i2c_priv[AUD3003O], &aud3003x_otp_regmap);
	if (IS_ERR(aud3003x->regmap[AUD3003O])) {
		dev_err(&i2c->dev, "Failed to allocate otp regmap: %li\n",
				PTR_ERR(aud3003x->regmap[AUD3003O]));
		ret = -ENOMEM;
		goto err;
	}

	i2c_set_clientdata(aud3003x->i2c_priv[AUD3003D], aud3003x);
	i2c_set_clientdata(aud3003x->i2c_priv[AUD3003A], aud3003x);
	i2c_set_clientdata(aud3003x->i2c_priv[AUD3003O], aud3003x);

	dev = &aud3003x->i2c_priv[AUD3003D]->dev;
	ret = devm_snd_soc_register_component(dev, &soc_codec_dev_aud3003x,
			aud3003x_dai, ARRAY_SIZE(aud3003x_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register digital codec: %d\n", ret);
		goto err;
	}

#ifdef CONFIG_PM
	pm_runtime_enable(aud3003x->dev);
#endif

	return ret;
err:
	kfree(aud3003x);
	return ret;
}

static int aud3003x_i2c_remove(struct i2c_client *i2c)
{
	struct aud3003x_priv *aud3003x = dev_get_drvdata(&i2c->dev);

	dev_dbg(aud3003x->dev, "(*) %s called\n", __func__);

	kfree(aud3003x);

	return 0;
}

static const struct i2c_device_id aud3003x_i2c_id[] = {
	{ "aud3003x", 3003 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aud3003x_i2c_id);

const struct of_device_id aud3003x_of_match[] = {
	{ .compatible = "codec,aud3003x", },
	{ },
};

static struct i2c_driver aud3003x_i2c_driver = {
	.driver = {
		.name = "aud3003x",
		.owner = THIS_MODULE,
		.pm = &aud3003x_pm,
		.of_match_table = of_match_ptr(aud3003x_of_match),
	},
	.probe = aud3003x_i2c_probe,
	.remove = aud3003x_i2c_remove,
	.id_table = aud3003x_i2c_id,
};

module_i2c_driver(aud3003x_i2c_driver);

MODULE_DESCRIPTION("ASoC AUD3003X driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:AUD3003X-codec");
