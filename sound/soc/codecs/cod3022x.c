/*
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *
 * Author: Sayanta Pattanayak <sayanta.p@samsung.com>
 *				<sayantapattanayak@gmail.com>
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
#include <sound/exynos_regmap_fw.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/completion.h>

#include <sound/s2801x.h>
#include <sound/cod3022x.h>
#include "cod3022x.h"

#define COD3022X_FIRMWARE_NAME	"cod3022x-s2801x-aud-fw.bin"

#define COD3022X_SAMPLE_RATE_48KHZ	48000
#define COD3022X_SAMPLE_RATE_192KHZ	192000

#define COD3022X_RESTORE_OTP_COUNT	5
#define COD3022X_RESTORE_REG_COUNT	16
#define COD3022X_OTP_R_OFFSET		0x20

#define COD3022X_MAX_IRQ_CHK_BITS	7
#define COD3022X_START_IRQ_CHK_BIT	2
#define COD3022X_MJ_DET_INVALID		(-1)

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err
#endif

struct cod3022x_priv *g_cod3022x;

static inline void cod3022x_usleep(unsigned int u_sec)
{
	usleep_range(u_sec, u_sec + 10);
}

/**
 * Helper functions to read ADC value for button detection
 */

#define COD3022X_ADC_SAMPLE_SIZE	5

static void cod3022x_adc_start(struct cod3022x_priv *cod3022x)
{
	cod3022x->jack_adc = iio_channel_get_all(cod3022x->dev);
}

static void cod3022x_adc_stop(struct cod3022x_priv *cod3022x)
{
	iio_channel_release(cod3022x->jack_adc);
}

static int cod3022x_adc_get_value(struct cod3022x_priv *cod3022x)
{
	int adc_data = -1;
	int adc_max = 0;
	int adc_min = 0xFFFF;
	int adc_total = 0;
	int adc_retry_cnt = 0;
	int i;
	struct iio_channel *jack_adc = cod3022x->jack_adc;

	for (i = 0; i < COD3022X_ADC_SAMPLE_SIZE; i++) {
		iio_read_channel_raw(&jack_adc[0], &adc_data);
		/* if adc_data is negative, ignore */
		while (adc_data < 0) {
			adc_retry_cnt++;
			if (adc_retry_cnt > 10)
				return adc_data;
			iio_read_channel_raw(&jack_adc[0], &adc_data);
		}

		/* Update min/max values */
		if (adc_data > adc_max)
			adc_max = adc_data;
		if (adc_data < adc_min)
			adc_min = adc_data;

		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (COD3022X_ADC_SAMPLE_SIZE - 2);
}

/**
 * Return value:
 * true: if the register value cannot be cached, hence we have to read from the
 * hardware directly
 * false: if the register value can be read from cache
 */
static bool cod3022x_volatile_register(struct device *dev, unsigned int reg)
{
	/**
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch(reg) {
	case COD3022X_01_IRQ1PEND ... COD3022X_04_IRQ4PEND:
	case COD3022X_09_STATUS1 ... COD3022X_0B_STATUS3:
	case COD3022X_61_RESERVED ... COD3022X_62_IRQ_R:
		return true;
	default:
		return false;
	}
}

/**
 * Return value:
 * true: if the register value can be read
 * flase: if the register cannot be read
 */
static bool cod3022x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case COD3022X_01_IRQ1PEND ... COD3022X_0B_STATUS3:
	case COD3022X_10_PD_REF ... COD3022X_1C_SV_DA:
	case COD3022X_20_VOL_AD1 ... COD3022X_24_DSM_ADS:
	case COD3022X_30_VOL_HPL ... COD3022X_38_DCT_CLK1:
	case COD3022X_40_DIGITAL_POWER ... COD3022X_44_ADC_R_VOL:
	case COD3022X_50_DAC1 ... COD3022X_5F_SPKLIMIT3:
	case COD3022X_60_OFFSET1 ... COD3022X_62_IRQ_R:
	case COD3022X_70_CLK1_AD ... COD3022X_7A_SL_DA2:
	case COD3022X_80_DET_PDB ... COD3022X_88_KEY_TIME:
	case COD3022X_F0_CTRL_IREF1 ... COD3022X_FE_CTRL_SPKS2:
		return true;
	default:
		return false;
	}
}

static bool cod3022x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	/* Reg-0x09 to Reg-0x0B are read-only status registers */
	case COD3022X_01_IRQ1PEND ... COD3022X_08_IRQ4M:
	case COD3022X_10_PD_REF ... COD3022X_1C_SV_DA:
	case COD3022X_20_VOL_AD1 ... COD3022X_24_DSM_ADS:
	case COD3022X_30_VOL_HPL ... COD3022X_38_DCT_CLK1:
	case COD3022X_40_DIGITAL_POWER ... COD3022X_44_ADC_R_VOL:
	case COD3022X_50_DAC1 ... COD3022X_5F_SPKLIMIT3:
	/* Reg-0x61 is reserved, Reg-0x62 is read-only */
	case COD3022X_60_OFFSET1:
	case COD3022X_70_CLK1_AD ... COD3022X_7A_SL_DA2:
	case COD3022X_80_DET_PDB ... COD3022X_88_KEY_TIME:
	case COD3022X_D0_CTRL_IREF1 ... COD3022X_DE_CTRL_SPKS2:
		return true;
	default:
		return false;
	}
}

const struct regmap_config cod3022x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = COD3022X_MAX_REGISTER,
	.readable_reg = cod3022x_readable_register,
	.writeable_reg = cod3022x_writeable_register,
	.volatile_reg = cod3022x_volatile_register,

	.cache_type = REGCACHE_RBTREE,
};

/* save the HP volume */
static int cod3022x_put_hp_vol(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_l = mc->reg;
	unsigned int reg_r = mc->rreg;
	int ret;

	ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;

	/* after update Read the volume from reg and store */
	cod3022x->vol_hpl = snd_soc_read(codec, reg_l);
	cod3022x->vol_hpr = snd_soc_read(codec, reg_r);

	return 0;
}

/* Function to set adc mixer control value */
static int cod3022x_soc_enum_put_adc_mix(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int val, reg_val;
	int ret;

	ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;

	val = (ucontrol->value.integer.value[0] & mask);
	mask = mask << shift;
	val = val << shift;
	reg_val = cod3022x->adc_mix_reg_val;
	reg_val &= ~mask;
	reg_val |= (val & mask);
	cod3022x->adc_mix_reg_val = reg_val;

	dev_dbg(codec->dev, "@%s: Reg=%02x RV=%02x, V=%02x, M=%02x, S=%02x\n",
			__func__, reg, reg_val, val, mask, shift);

	return 0;
}

/* Function to get adc mixer control value */
static int cod3022x_soc_enum_get_adc_mix(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;

	dev_dbg(codec->dev, "%s called\n", __func__);
	ucontrol->value.integer.value[0] = (cod3022x->adc_mix_reg_val >> shift)
							& mask;

	return 0;
}

/**
 * TLV_DB_SCALE_ITEM
 *
 * (TLV: Threshold Limit Value)
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

/**
 * cod3022x_ctvol_bst_tlv
 *
 * Map: (0x0, 0dB), (0x1, 12dB), (0x2, 20dB)
 *
 * CTVOL_BST1, reg(0x20), shift(5), width(2)
 * CTVOL_BST2, reg(0x21), shift(5), width(2)
 */
static const unsigned int cod3022x_ctvol_bst_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 1, TLV_DB_SCALE_ITEM(0, 1200, 0),
	2, 2, TLV_DB_SCALE_ITEM(2000, 0, 0),
};

/**
 * cod3022x_ctvol_bst_pga_tlv
 *
 * Range: -16.5dB to +18dB, step 1.5dB
 *
 * CTVOL_BST_PGA1, reg(0x20), shift(0), width(5), invert(1), max(31)
 * CTVOL_BST_PGA2, reg(0x21), shift(0), width(5), invert(1), max(31)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_bst_pga_tlv, -1650, 150, 0);

/**
 * cod3022x_ctvol_line_tlv
 *
 * Range: -12dB to +6dB, step 3dB
 *
 * This range is used for following controls
 * CTVOL_LNL, reg(0x22), shift(4), width(3), invert(0), max(6)
 * CTVOL_LNR, reg(0x22), shift(0), width(3), invert(0), max(6)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_line_tlv, -1200, 300, 0);

/**
 * cod3022x_ctvol_hp_tlv
 *
 * Range: -57dB to +6dB, step 1dB
 *
 * CTVOL_HPL, reg(0x30), shift(0), width(6), invert(1), max(63)
 * CTVOL_HPR, reg(0x31), shift(0), width(6), invert(1), max(63)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_hp_tlv, -5700, 100, 0);

/**
 * cod3019_ctvol_ep_tlv
 *
 * Range: 0dB to +12dB, step 1dB
 *
 * CTVOL_EP, reg(0x32), shift(4), width(4), invert(0), max(12)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_ep_tlv, 0, 100, 0);

/**
 * cod3022x_ctvol_spk_pga_tlv
 *
 * Range: -6dB to +3dB, step 1dB
 *
 * CTVOL_SPK_PGA, reg(0x32), shift(0), width(4), invert(0), max(9)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_spk_pga_tlv, -600, 100, 0);

/**
 * cod3022x_dvol_adc_tlv
 *
 * Map as per data-sheet:
 * (0x00 to 0x86) -> (+12dB to -55dB, step 0.5dB)
 * (0x87 to 0x91) -> (-56dB to -66dB, step 1dB)
 * (0x92 to 0x94) -> (-68dB to -72dB, step 2dB)
 * (0x95 to 0x96) -> (-78dB to -84dB, step 6dB)
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_ADL, reg(0x43), shift(0), width(8), invert(1), max(0x96)
 * DVOL_ADR, reg(0x44), shift(0), width(8), invert(1), max(0x96)
 * DVOL_DAL, reg(0x51), shift(0), width(8), invert(1), max(0x96)
 * DVOL_DAR, reg(0x52), shift(0), width(8), invert(1), max(0x96)
 */
static const unsigned int cod3022x_dvol_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x00, 0x01, TLV_DB_SCALE_ITEM(-8400, 600, 0),
	0x02, 0x04, TLV_DB_SCALE_ITEM(-7200, 200, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-6600, 100, 0),
	0x10, 0x96, TLV_DB_SCALE_ITEM(-5500, 50, 0),
};

/**
 * cod3022x_dnc_min_gain_tlv
 *
 * Range: -6dB to 0dB, step 1dB
 *
 * DNC_MINGAIN , reg(0x55), shift(5), width(3)
 */
static const unsigned int cod3022x_dnc_min_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x00, 0x06, TLV_DB_SCALE_ITEM(-600, 0, 0),
};

/**
 * cod3022x_dnc_max_gain_tlv
 *
 * Range: 0dB to 24dB, step 1dB
 *
 * DNC_MAXGAIN , reg(0x55), shift(0), width(5)
 */
static const unsigned int cod3022x_dnc_max_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x06, 0x1e, TLV_DB_SCALE_ITEM(0, 2400, 0),
};

/**
 * cod3022x_dnc_lvl_tlv
 *
 * Range: -10.5dB to 0dB, step 1.5dB
 *
 * DNCLVL_R/L, reg(0x55), shift(0/4), width(3), invert(0), max(7)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_dnc_lvl_tlv, -1050, 0, 0);

/**
 * mono_mix_mode
 *
 * Selecting the Mode of Mono Mixer (inside DAC block)
 */
static const char *cod3022x_mono_mix_mode_text[] = {
	"Disable", "R", "L", "LR-Invert",
	"(L+R)/2", "L+R"
};

static const struct soc_enum cod3022x_mono_mix_mode_enum =
	SOC_ENUM_SINGLE(COD3022X_50_DAC1, DAC1_MONOMIX_SHIFT,
			ARRAY_SIZE(cod3022x_mono_mix_mode_text),
			cod3022x_mono_mix_mode_text);

/**
 * chargepump_mode
 *
 * Selecting the chargepump mode
 */
static const char *cod3022x_chargepump_mode_text[] = {
	"VDD", "HALF-VDD", "CLASS-G-D", "CLASS-G-A"
};

static const struct soc_enum cod3022x_chargepump_mode_enum =
	SOC_ENUM_SINGLE(COD3022X_33_CTRL_EP, CTMV_CP_MODE_SHIFT,
			ARRAY_SIZE(cod3022x_chargepump_mode_text),
			cod3022x_chargepump_mode_text);

static int dac_soft_mute_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int dac_soft_mute_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int value =ucontrol->value.integer.value[0];

	if (g_cod3022x == NULL)
		return -1;

	if (!value)
		/* enable soft mute */
		snd_soc_update_bits(g_cod3022x->codec, COD3022X_50_DAC1,
			DAC1_SOFT_MUTE_MASK, DAC1_SOFT_MUTE_MASK);
	else
		/* diable soft mute */
		snd_soc_update_bits(g_cod3022x->codec, COD3022X_50_DAC1,
			DAC1_SOFT_MUTE_MASK, 0x0);

	dev_info(g_cod3022x->codec->dev, "%s: soft mute : %s\n", __func__, 
		(!value) ? "on":"off");
	return 0;
}

/**
 * struct snd_kcontrol_new cod3022x_snd_control
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

/**
 * All the data goes into cod3022x_snd_controls.
 * All path inter-connections goes into cod3022x_dapm_routes
 */
static const struct snd_kcontrol_new cod3022x_snd_controls[] = {
	SOC_SINGLE_TLV("MIC1 Boost Volume", COD3022X_20_VOL_AD1,
			VOLAD1_CTVOL_BST1_SHIFT,
			(BIT(VOLAD1_CTVOL_BST1_WIDTH) - 1), 0,
			cod3022x_ctvol_bst_tlv),

	SOC_SINGLE_TLV("MIC1 Volume", COD3022X_20_VOL_AD1,
			VOLAD1_CTVOL_BST_PGA1_SHIFT,
			(BIT(VOLAD1_CTVOL_BST_PGA1_WIDTH) - 1), 1,
			cod3022x_ctvol_bst_pga_tlv),

	SOC_SINGLE_TLV("MIC2 Boost Volume", COD3022X_21_VOL_AD2,
			VOLAD2_CTVOL_BST2_SHIFT,
			(BIT(VOLAD2_CTVOL_BST2_WIDTH) - 1), 0,
			cod3022x_ctvol_bst_tlv),

	SOC_SINGLE_TLV("MIC2 Volume", COD3022X_21_VOL_AD2,
			VOLAD2_CTVOL_BST_PGA2_SHIFT,
			(BIT(VOLAD2_CTVOL_BST_PGA2_WIDTH) - 1), 1,
			cod3022x_ctvol_bst_pga_tlv),

	SOC_DOUBLE_TLV("Line-in Volume", COD3022X_22_VOL_AD3,
			VOLAD3_CTVOL_LNL_SHIFT, VOLAD3_CTVOL_LNR_SHIFT,
			(BIT(VOLAD3_CTVOL_LNL_WIDTH) - 1), 0,
			cod3022x_ctvol_line_tlv),

	SOC_DOUBLE_R_EXT_TLV("Headphone Volume", COD3022X_30_VOL_HPL,
			COD3022X_31_VOL_HPR, VOLHP_CTVOL_HP_SHIFT,
			(BIT(VOLHP_CTVOL_HP_WIDTH) - 1), 1, snd_soc_get_volsw,
			cod3022x_put_hp_vol, cod3022x_ctvol_hp_tlv),

	SOC_SINGLE_TLV("Earphone Volume", COD3022X_32_VOL_EP_SPK,
			CTVOL_EP_SHIFT,
			(BIT(CTVOL_EP_WIDTH) - 1), 0,
			cod3022x_ctvol_ep_tlv),

	SOC_SINGLE_TLV("Speaker Volume", COD3022X_32_VOL_EP_SPK,
			CTVOL_SPK_PGA_SHIFT,
			(BIT(CTVOL_SPK_PGA_WIDTH) - 1), 0,
			cod3022x_ctvol_spk_pga_tlv),

	SOC_SINGLE_TLV("ADC Left Gain", COD3022X_43_ADC_L_VOL,
			AD_DA_DVOL_SHIFT,
			AD_DA_DVOL_MAXNUM, 1, cod3022x_dvol_tlv),

	SOC_SINGLE_TLV("ADC Right Gain", COD3022X_44_ADC_R_VOL,
			AD_DA_DVOL_SHIFT,
			AD_DA_DVOL_MAXNUM, 1, cod3022x_dvol_tlv),

	SOC_DOUBLE_R_TLV("DAC Gain", COD3022X_51_DAC_L_VOL,
			COD3022X_52_DAC_R_VOL, AD_DA_DVOL_SHIFT,
			AD_DA_DVOL_MAXNUM, 1, cod3022x_dvol_tlv),

	SOC_SINGLE_TLV("DNC Min Gain", COD3022X_55_DNC2,
			DNC_MIN_GAIN_SHIFT,
			(BIT(DNC_MIN_GAIN_WIDTH) - 2), 0,
			cod3022x_dnc_min_gain_tlv),

	SOC_SINGLE_TLV("DNC Max Gain", COD3022X_55_DNC2,
			DNC_MAX_GAIN_SHIFT,
			(BIT(DNC_MAX_GAIN_WIDTH) - 2), 0,
			cod3022x_dnc_max_gain_tlv),

	SOC_DOUBLE_TLV("DNC Level", COD3022X_56_DNC3,
			DNC_LVL_L_SHIFT, DNC_LVL_R_SHIFT,
			(BIT(DNC_LVL_L_WIDTH) - 1), 0, cod3022x_dnc_lvl_tlv),

	SOC_ENUM("MonoMix Mode", cod3022x_mono_mix_mode_enum),

	SOC_ENUM("Chargepump Mode", cod3022x_chargepump_mode_enum),

	SOC_SINGLE_EXT("ADCL Mixer MIC1L Switch", COD3022X_23_MIX_AD, EN_MIX_MIC1L_SHIFT,
				1, 0, cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),

	SOC_SINGLE_EXT("ADCL Mixer MIC2L Switch", COD3022X_23_MIX_AD, EN_MIX_MIC2L_SHIFT,
				1, 0,cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),

	/* LINE-IN Left Channel Input for ADCL */
	SOC_SINGLE_EXT("ADCL Mixer LINELL Switch", COD3022X_23_MIX_AD, EN_MIX_LNLL_SHIFT,
				1, 0,cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),

	/* LINE-IN Right Channel Input for ADCL */
	SOC_SINGLE_EXT("ADCL Mixer LINERL Switch", COD3022X_23_MIX_AD, EN_MIX_LNRL_SHIFT,
				1, 0, cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),

	SOC_SINGLE_EXT("ADCR Mixer MIC1R Switch", COD3022X_23_MIX_AD, EN_MIX_MIC1R_SHIFT,
				1, 0, cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),

	SOC_SINGLE_EXT("ADCR Mixer MIC2R Switch", COD3022X_23_MIX_AD, EN_MIX_MIC2R_SHIFT,
				1, 0, cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),

	/* LINE-IN Left Channel Input for ADCR */
	SOC_SINGLE_EXT("ADCR Mixer LINELR Switch", COD3022X_23_MIX_AD, EN_MIX_LNLR_SHIFT,
				1, 0, cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),

	/* LINE-IN Right Channel Input for ADCR */
	SOC_SINGLE_EXT("ADCR Mixer LINERR Switch", COD3022X_23_MIX_AD, EN_MIX_LNRR_SHIFT,
				1, 0, cod3022x_soc_enum_get_adc_mix,
				cod3022x_soc_enum_put_adc_mix),
				
	SOC_SINGLE_EXT("DAC Soft Mute",SND_SOC_NOPM, 0, 100, 0,
			dac_soft_mute_get, dac_soft_mute_put),				
};

static int dac_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		int event)
{
	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Default value of DIGITAL BLOCK */
		snd_soc_write(w->codec, COD3022X_40_DIGITAL_POWER, 0xf9);

		/* DAC digital power On */
		snd_soc_update_bits(w->codec, COD3022X_40_DIGITAL_POWER,
				PDB_DACDIG_MASK | RSTB_OVFW_DA_MASK,
				PDB_DACDIG_MASK);

		snd_soc_update_bits(w->codec, COD3022X_40_DIGITAL_POWER,
				RSTB_DAT_DA_MASK, 0x0);

		snd_soc_update_bits(w->codec, COD3022X_40_DIGITAL_POWER,
				RSTB_DAT_DA_MASK, RSTB_DAT_DA_MASK);

		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* Default value of DIGITAL BLOCK during power off */
		snd_soc_write(w->codec, COD3022X_40_DIGITAL_POWER, 0xe9);
		break;

	default:
		break;
	}

	return 0;
}

static int cod3022x_adc_mute_use_count(struct snd_soc_codec *codec, bool on)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	int process_event = 0;

	if(on) {
		atomic_inc(&cod3022x->adc_mute_use_count);
		if(atomic_read(&cod3022x->adc_mute_use_count) == 1)
			process_event = 1;
	} else {
		atomic_dec(&cod3022x->adc_mute_use_count);
		if(atomic_read(&cod3022x->adc_mute_use_count) == 0)
			process_event = 1;
	}

	dev_dbg(codec->dev, "%s called, mute process_evnet = %d\n", __func__, process_event);
	return process_event;
}

static void cod3022x_adc_digital_mute(struct snd_soc_codec *codec, bool on)
{
	dev_dbg(codec->dev, "%s called, on = %d\n", __func__, on);
	if(!cod3022x_adc_mute_use_count(codec, on))
		return;

	if (on)
		snd_soc_update_bits(codec, COD3022X_42_ADC1,
				ADC1_MUTE_AD_EN_MASK, ADC1_MUTE_AD_EN_MASK);
	else
		snd_soc_update_bits(codec, COD3022X_42_ADC1,
				ADC1_MUTE_AD_EN_MASK, 0);
}

/**
 * cod3022x_mute_mic2
 * Mute mic2 if it is active
 *
 * Returns -1 if error, else 0
 */
static int cod3022x_mute_mic2(bool on)
{
	int mic_on;

	if (g_cod3022x == NULL)
		return -1;

	mic_on = snd_soc_read(g_cod3022x->codec, COD3022X_75_CHOP_AD);

	/* Check mic2 active, if mic2 is not active return */
	if (!(mic_on & EN_MCB2_CHOP_MASK))
		return 0;

	dev_dbg(g_cod3022x->codec->dev, "%s called, %s\n", __func__,
			on ? "Mute" : "Unmute");

	if (on) {
		cod3022x_adc_digital_mute(g_cod3022x->codec, true);
		snd_soc_update_bits(g_cod3022x->codec, COD3022X_12_PD_AD2,
							PDB_MIC2_MASK, 0);
	} else {
		snd_soc_update_bits(g_cod3022x->codec, COD3022X_12_PD_AD2,
					PDB_MIC2_MASK, PDB_MIC2_MASK);
		cod3022x_adc_digital_mute(g_cod3022x->codec, false);
	}

	return 0;
}

/* process the button events based on the need */
void cod3022x_process_button_ev(int code, int on)
{
	bool key_press = on ? true : false;

	cod3022x_mute_mic2(key_press);
}

static void cod3022x_capture_init_manual_mode(struct snd_soc_codec *codec)
{

	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_VMID_MASK, PDB_VMID_MASK);

	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
					CTMF_VMID_MASK,
					CTMF_VMID_5K_OM << CTMF_VMID_SHIFT);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			EN_DSMR_PREQ_MASK | EN_DSML_PREQ_MASK,
			EN_DSMR_PREQ_MASK | EN_DSML_PREQ_MASK);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_IGEN_MASK, PDB_IGEN_MASK);
}

static int cod3022x_capture_init(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_write(codec, COD3022X_D0_CTRL_IREF1, 0x0);

	/* As per SFR v1.14, enable ADC digital mute before configuring ADC */
	cod3022x_adc_digital_mute(codec, true);

	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
			PDB_ADCDIG_MASK | RSTB_OVFW_DA_MASK,
			PDB_ADCDIG_MASK);

	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
			RSTB_DAT_AD_MASK, 0x0);

	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
			RSTB_DAT_AD_MASK, RSTB_DAT_AD_MASK);

	if (!cod3022x->use_auto_mic_power_on)
		cod3022x_capture_init_manual_mode(codec);

	return 0;
}

static void cod3022x_capture_deinit_manual_mode(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_IGEN_AD_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_IGEN_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_VMID_MASK, 0);
}

static int cod3022x_capture_deinit(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	int dac_on;

	dac_on = snd_soc_read(codec, COD3022X_40_DIGITAL_POWER);

	if (PDB_DACDIG_MASK & dac_on)
		snd_soc_update_bits(codec,
				COD3022X_40_DIGITAL_POWER,
				RSTB_DAT_AD_MASK, 0x0);
	else
		snd_soc_update_bits(codec,
				COD3022X_40_DIGITAL_POWER,
				RSTB_DAT_AD_MASK | RSTB_OVFW_DA_MASK,
				RSTB_OVFW_DA_MASK);

	if (!cod3022x->use_auto_mic_power_on)
		cod3022x_capture_deinit_manual_mode(codec);

	/* As per SFR v1.14, disable ADC digital mute after configuring ADC */
	cod3022x_adc_digital_mute(codec, false);

	return 0;
}

static int adc_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		int event)
{
	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;

	case SND_SOC_DAPM_POST_PMU:
		/* As per SFR v1.14, disable ADC digital mute after configuring ADC */
		cod3022x_adc_digital_mute(w->codec, false);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* As per SFR v1.14, enable ADC digital mute before configuring ADC */
		cod3022x_adc_digital_mute(w->codec, true);
		break;

	default:
		break;
	}

	return 0;
}

int cod3022x_mic_bias_ev(struct snd_soc_codec *codec, int mic, int event)
{
	int is_other_mic_on, mask;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s Called, Mic = %d, Event = %d\n",
				__func__, mic, event);

	is_other_mic_on = snd_soc_read(codec, COD3022X_10_PD_REF);
	if (mic == COD3022X_MIC1) {
		is_other_mic_on &= PDB_MCB2_MASK;
		mask = is_other_mic_on ? PDB_MCB1_MASK :
			PDB_MCB1_MASK | PDB_MCB_LDO_CODEC_MASK;
	} else if (mic == COD3022X_MIC2) {
		is_other_mic_on &= PDB_MCB1_MASK;
		mask = is_other_mic_on ? PDB_MCB2_MASK :
			PDB_MCB2_MASK | PDB_MCB_LDO_CODEC_MASK;
	} else {
		dev_err(codec->dev, "%s Called , Invalid MIC ID\n",
							__func__);
		return -1;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, COD3022X_10_PD_REF, mask, mask);
		if ((cod3022x->use_auto_mic_power_on) && (mic == COD3022X_MIC2))
				snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
						CTRM_MCB2_MASK, CTRM_MCB2_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, COD3022X_10_PD_REF, mask, 0x00);

		if ((cod3022x->use_auto_mic_power_on) && (mic == COD3022X_MIC2))
			snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
							CTRM_MCB2_MASK, 0);
		break;
	}

	return 0;
}

static void cod3022x_auto_power_on_mic1(struct snd_soc_codec *codec)
{
	unsigned int mix_val;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	mix_val = cod3022x->adc_mix_reg_val;
	mix_val &= EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK;

	/* Select default if no paths have been selected */
	if (!mix_val)
		mix_val = EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK;

	/* Reset the mixer-switches before powering on */
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK, 0x0);

	cod3022x_usleep(100);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_MIC1_MASK,
			APW_AUTO_AD_MASK | APW_MIC1_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK, mix_val);

	/* 140 ms delay recommended by MSC Team */
	msleep(140);
}

static void cod3022x_manual_power_on_mic1(struct snd_soc_codec *codec)
{
	unsigned int mix_val;
	unsigned int ad_val, d3_val, d0_val;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	unsigned int vol_mic1;

	mix_val = cod3022x->adc_mix_reg_val;

	/* Reset the mixer-switches before powering on */
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK, 0x0);

	cod3022x_usleep(100);

	snd_soc_update_bits(codec, COD3022X_75_CHOP_AD,
					EN_MIC_CHOP_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_IGEN_AD_MASK, PDB_IGEN_AD_MASK);

	d0_val = snd_soc_read(codec, COD3022X_D0_CTRL_IREF1);
	d3_val = snd_soc_read(codec, COD3022X_D3_CTRL_IREF4);
	ad_val = snd_soc_read(codec, COD3022X_12_PD_AD2);

	snd_soc_update_bits(codec, COD3022X_D3_CTRL_IREF4,
			CTMI_LN_MASK, CTMI_LN_4_5U);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_LNR_MASK, PDB_LNR_MASK);

	snd_soc_update_bits(codec, COD3022X_D0_CTRL_IREF1,
			CTMI_MIX_MASK, CTMI_MIX_4_5U);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIXL_MASK | PDB_MIXR_MASK,
			PDB_MIXL_MASK | PDB_MIXR_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK,
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK);

	vol_mic1 = snd_soc_read(codec, COD3022X_20_VOL_AD1);
	snd_soc_update_bits(codec, COD3022X_20_VOL_AD1,
				VOLAD1_CTVOL_BST1_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK,
			((mix_val & (EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK)) ?
			(mix_val & (EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK)) :
			(EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK)));

	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
					CTMF_VMID_MASK,
					CTMF_VMID_50K_OM << CTMF_VMID_SHIFT);

	msleep(60);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC1_MASK, PDB_MIC1_MASK);

	snd_soc_update_bits(codec, COD3022X_20_VOL_AD1,
				VOLAD1_CTVOL_BST1_MASK,
				vol_mic1 & VOLAD1_CTVOL_BST1_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK,
			mix_val & (EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK));

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_LNR_MASK ,
			(ad_val & (PDB_LNR_MASK)));

	snd_soc_update_bits(codec, COD3022X_D3_CTRL_IREF4,
			CTMI_LN_MASK, d3_val);

	snd_soc_update_bits(codec, COD3022X_D0_CTRL_IREF1,
			CTMI_MIX_MASK, d0_val);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_DSMR_MASK | PDB_DSML_MASK,
			PDB_DSMR_MASK | PDB_DSML_MASK);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			EN_DSMR_PREQ_MASK | EN_DSML_PREQ_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_RESETB_DSML_DSMR_MASK ,
			PDB_RESETB_DSML_DSMR_MASK);
	msleep(100);

	/* reset Mic power */
	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC1_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC1_MASK, PDB_MIC1_MASK);

	snd_soc_update_bits(codec, COD3022X_75_CHOP_AD,
				EN_MIC_CHOP_MASK, EN_MIC_CHOP_MASK);
}

static int cod3022_power_on_mic1(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called\n", __func__);

	if (cod3022x->use_auto_mic_power_on)
		cod3022x_auto_power_on_mic1(codec);
	else
		cod3022x_manual_power_on_mic1(codec);

	return 0;
}

static void cod3022x_auto_power_on_mic2(struct snd_soc_codec *codec)
{
	unsigned int mix_val;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	mix_val = cod3022x->adc_mix_reg_val;
	mix_val &= EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK;

	/* Select default if no paths have been selected */
	if (!mix_val)
		mix_val = EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK;

	/* Reset the mixer-switches before powering on */
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK, 0x0);

	cod3022x_usleep(100);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_MIC2_MASK,
			APW_AUTO_AD_MASK | APW_MIC2_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK, mix_val);

	/* 140 ms delay recommended by MSC Team */
	msleep(140);
}

static void cod3022x_manual_power_on_mic2(struct snd_soc_codec *codec)
{
	unsigned int mix_val;
	unsigned int ad_val, d3_val, d0_val;
	unsigned int vol_mic2;
	unsigned int detb_period = CTMF_DETB_PERIOD_2048;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	if (cod3022x->use_external_jd || cod3022x->use_btn_adc_mode)
		detb_period = CTMF_DETB_PERIOD_8;

	mix_val = cod3022x->adc_mix_reg_val;

	/* Reset the mixer-switches before powering on */
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK, 0x0);

	cod3022x_usleep(100);

	snd_soc_update_bits(codec, COD3022X_75_CHOP_AD,
				EN_MIC_CHOP_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_IGEN_AD_MASK, PDB_IGEN_AD_MASK);

	d0_val = snd_soc_read(codec, COD3022X_D0_CTRL_IREF1);
	d3_val = snd_soc_read(codec, COD3022X_D3_CTRL_IREF4);
	ad_val = snd_soc_read(codec, COD3022X_12_PD_AD2);

	snd_soc_update_bits(codec, COD3022X_D3_CTRL_IREF4,
			CTMI_LN_MASK, CTMI_LN_4_5U);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				 PDB_LNR_MASK, PDB_LNR_MASK);

	snd_soc_update_bits(codec, COD3022X_D0_CTRL_IREF1,
			CTMI_MIX_MASK, CTMI_MIX_4_5U);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIXL_MASK | PDB_MIXR_MASK,
			PDB_MIXL_MASK | PDB_MIXR_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK,
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK);

	vol_mic2 = snd_soc_read(codec, COD3022X_21_VOL_AD2);
	snd_soc_update_bits(codec, COD3022X_21_VOL_AD2,
				VOLAD2_CTVOL_BST2_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK,
			((mix_val & (EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK)) ?
			(mix_val & (EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK)) :
			(EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK)));

	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
					CTMF_VMID_MASK,
					CTMF_VMID_50K_OM << CTMF_VMID_SHIFT);
	msleep(60);

	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
			CTRM_MCB2_MASK, CTRM_MCB2_MASK);

	msleep(1);
	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC2_MASK, PDB_MIC2_MASK);

	snd_soc_update_bits(codec, COD3022X_21_VOL_AD2,
				VOLAD2_CTVOL_BST2_MASK,
				vol_mic2 & VOLAD2_CTVOL_BST2_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK,
			mix_val & (EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK));

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_LNR_MASK, (ad_val & (PDB_LNR_MASK)));

	snd_soc_update_bits(codec, COD3022X_D3_CTRL_IREF4,
			CTMI_LN_MASK, d3_val);

	snd_soc_update_bits(codec, COD3022X_D0_CTRL_IREF1,
			CTMI_MIX_MASK, d0_val);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_DSMR_MASK | PDB_DSML_MASK,
			PDB_DSMR_MASK | PDB_DSML_MASK);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			EN_DSMR_PREQ_MASK | EN_DSML_PREQ_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_RESETB_DSML_DSMR_MASK ,
			PDB_RESETB_DSML_DSMR_MASK);
	msleep(100);

	/* reset Mic power */
	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC2_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC2_MASK, PDB_MIC2_MASK);

	snd_soc_update_bits(codec, COD3022X_75_CHOP_AD,
				EN_MIC_CHOP_MASK, EN_MIC_CHOP_MASK);
}

static int cod3022_power_on_mic2(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called\n", __func__);

	if (cod3022x->use_auto_mic_power_on)
		cod3022x_auto_power_on_mic2(codec);
	else
		cod3022x_manual_power_on_mic2(codec);

	return 0;
}


static void cod3022x_auto_power_off_mic1(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_MIC1_MASK, 0);
}

static void cod3022x_manual_power_off_mic1(struct snd_soc_codec *codec)
{
	int other_mic;

	other_mic = snd_soc_read(codec, COD3022X_75_CHOP_AD);
	other_mic &= (EN_MCB2_CHOP_MASK | EN_LN_CHOP_MASK);

	if (!other_mic)
		snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_RESETB_DSML_DSMR_MASK ,
			PDB_DSML_MASK | PDB_DSMR_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK, 0);

	if (!other_mic) {
		snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_DSML_MASK | PDB_DSMR_MASK, 0);

		snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIXL_MASK | PDB_MIXR_MASK, 0);
	}

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC1_MASK, 0);

}

static int cod3022_power_off_mic1(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "%s called\n", __func__);

	if (cod3022x->use_auto_mic_power_on)
		cod3022x_auto_power_off_mic1(codec);
	else
		cod3022x_manual_power_off_mic1(codec);

	dev_dbg(codec->dev, "%s completed\n", __func__);
	return 0;
}

static void cod3022x_auto_power_off_mic2(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_MIC2_MASK, 0);
}

static void cod3022x_manual_power_off_mic2(struct snd_soc_codec *codec)
{
	int other_mic;

	other_mic = snd_soc_read(codec, COD3022X_75_CHOP_AD);
	other_mic &= (EN_MCB1_CHOP_MASK | EN_LN_CHOP_MASK);

	if (!other_mic)
		snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_RESETB_DSML_DSMR_MASK ,
			PDB_DSML_MASK | PDB_DSMR_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK, 0);

	if (!other_mic) {
		snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
			PDB_DSML_MASK | PDB_DSMR_MASK, 0);

		snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIXL_MASK | PDB_MIXR_MASK, 0);
	}

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
			PDB_MIC2_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
					CTRM_MCB2_MASK, 0);
}

static int cod3022_power_off_mic2(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "%s called\n", __func__);

	if (cod3022x->use_auto_mic_power_on)
		cod3022x_auto_power_off_mic2(codec);
	else
		cod3022x_manual_power_off_mic2(codec);

	dev_dbg(codec->dev, "%s completed\n", __func__);
	return 0;
}

static int cod3022_power_on_linein(struct snd_soc_codec *codec)
{
	unsigned int mix_val;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called\n", __func__);

	mix_val = cod3022x->adc_mix_reg_val;
	mix_val &= EN_MIX_LNLL_MASK | EN_MIX_LNLR_MASK |
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK;

	/* Select default if no paths have been selected left to mix left
	 and right to mix right */
	if (!mix_val)
		mix_val = EN_MIX_LNLL_MASK | EN_MIX_LNRR_MASK;

	/* Reset the mixer-switches before powering on */
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_LNLL_MASK | EN_MIX_LNLR_MASK |
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK,
			0x0);

	cod3022x_usleep(100);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_LN_MASK,
			APW_AUTO_AD_MASK | APW_LN_MASK);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_LNLL_MASK | EN_MIX_LNLR_MASK |
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK,
			mix_val);

	/* 250 ms delay recommended by MSC Team */
	msleep(250);

	return 0;
}

static int cod3022_power_off_linein(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
			EN_MIX_LNLL_MASK | EN_MIX_LNLR_MASK |
			EN_MIX_LNRL_MASK | EN_MIX_LNRR_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_LN_MASK, 0);

	return 0;
}

static int vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{

	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022x_capture_init(w->codec);
		break;

	case SND_SOC_DAPM_POST_PMU:
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod3022x_capture_deinit(w->codec);
		break;

	default:
		break;
	}

	return 0;
}

/**
 * SFR revision 1.14 recommends following settings.
 * If playback is only HP mode, write f/w OTP value at 0xD4 and 0xD5. Enable DNC.
 * If playback is not only HP mode, write all zero data value at 0xD4 and 0xD5
 * with DNC disabled.
 */
static void cod3022x_update_playback_otp(struct snd_soc_codec *codec)
{
	int hp_on, spk_on, ep_on;
	int chop_val;
	int hp_pw_sts;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	chop_val = snd_soc_read(codec, COD3022X_76_CHOP_DA);
	hp_on = chop_val & EN_HP_CHOP_MASK;
	spk_on =  chop_val & EN_SPK_PGA_CHOP_MASK;
	ep_on =  chop_val & EN_EP_CHOP_MASK;

	if (!hp_on && !spk_on && !ep_on) {
		dev_warn(codec->dev, "None of the output paths selected.\n");
		return;
	}

	/* Set D4 and D5 to 0 for all playback mode */
	snd_soc_write(codec, COD3022X_D4_OFFSET_DAL, 0x0);
	snd_soc_write(codec, COD3022X_D5_OFFSET_DAR, 0x0);

	if (spk_on || ep_on) {
		/* Disable DNC */
		snd_soc_update_bits(codec, COD3022X_54_DNC1,
				EN_DNC_MASK , 0x0);

		cod3022x_usleep(100);

		hp_pw_sts = snd_soc_read(codec, COD3022X_17_PWAUTO_DA);

		if (hp_pw_sts & APW_HP_MASK) {
			/* Either SPK or EP is on, and hp is already on then set given
			 * analog HP volume
			 */
			snd_soc_write(codec, COD3022X_30_VOL_HPL,
							cod3022x->vol_hpl);
			snd_soc_write(codec, COD3022X_31_VOL_HPR,
							cod3022x->vol_hpr);
		}
	}

	if (ep_on) {
		/**
		 * When EP path is enabled, update 0xDC as 0x58.
		 * CTMI_EP_A: 0x1 (2.0 uA)
		 * CTMI_EP_P: 0x3 (4.0 uA)
		 * CTMI_EP_D: 0x0 (2.0 uA)
		 */
		snd_soc_write(codec, COD3022X_DC_CTRL_EPS,
				(CTMI_EP_A_2_UA << CTMI_EP_A_SHIFT) |
				(CTMI_EP_P_D_4_UA << CTMI_EP_P_SHIFT) |
				(CTMI_EP_P_D_2_UA << CTMI_EP_D_SHIFT));
	}
}

static int cod3022x_hp_playback_init(struct snd_soc_codec *codec)
{
	unsigned int mix_val;

	dev_dbg(codec->dev, "%s called\n", __func__);

	/* Set HP current to 4uA always */
	snd_soc_update_bits(codec, COD3022X_DB_CTRL_HPS, CTMI_HP_A_MASK,
				CTMI_HP_4_UA << CTMI_HP_A_SHIFT);

	mix_val = snd_soc_read(codec, COD3022X_36_MIX_DA1);

	/* Enable DAC path by default */
	mix_val |= EN_HP_MIXL_DCTL_MASK | EN_HP_MIXR_DCTR_MASK;

	snd_soc_write(codec, COD3022X_36_MIX_DA1, 0x0);

	snd_soc_update_bits(codec, COD3022X_D7_CTRL_CP1,
			CTRV_CP_NEGREF_MASK, 0x00);

	/* Enable DNC Start gain*/
	snd_soc_update_bits(codec, COD3022X_54_DNC1,
				DNC_START_GAIN_MASK, DNC_START_GAIN_MASK);

	/* Set DNC Start gain value*/
	snd_soc_write(codec, COD3022X_5A_DNC7, 0x18);

	snd_soc_update_bits(codec, COD3022X_54_DNC1,
					EN_DNC_MASK, EN_DNC_MASK);

	cod3022x_usleep(100);

	snd_soc_update_bits(codec, COD3022X_54_DNC1,
					EN_DNC_MASK, 0);

	/* set HP volume Level */
	snd_soc_write(codec, COD3022X_30_VOL_HPL, 0x26);
	snd_soc_write(codec, COD3022X_31_VOL_HPR, 0x26);

	/* Update OTP configuration */
	cod3022x_update_playback_otp(codec);

	/* DNC Target level selection */
	snd_soc_write(codec, COD3022X_56_DNC3, 0x33);

	/* DNC Window selection set to 20Hz time window */
	snd_soc_update_bits(codec, COD3022X_57_DNC4, DNC_WINSEL_MASK,
				(DNC_WIN_SIZE_20HZ << DNC_WINSEL_SHIFT));

	snd_soc_write(codec, COD3022X_36_MIX_DA1, mix_val);

	cod3022x_usleep(100);

	return 0;
}

static int spkdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	unsigned int spk_on;
	unsigned int spk_gain;
	unsigned int mix_val;
	unsigned int hp_on;

	spk_on = snd_soc_read(w->codec, COD3022X_76_CHOP_DA);
	if (!(spk_on & EN_SPK_PGA_CHOP_MASK)) {
		dev_dbg(w->codec->dev, "%s called but speaker not enabled\n",
				__func__);
		return 0;
	}

	dev_dbg(w->codec->dev, "%s called event=%d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Update OTP configuration */
		cod3022x_update_playback_otp(w->codec);

		mix_val = snd_soc_read(w->codec, COD3022X_37_MIX_DA2);
		mix_val &= EN_SPK_MIX_DCTL_MASK | EN_SPK_MIX_DCTR_MASK |
			EN_SPK_MIX_MIXL_MASK | EN_SPK_MIX_MIXR_MASK;

		/* Enable DAC path by default */
		mix_val |= EN_SPK_MIX_DCTL_MASK | EN_SPK_MIX_DCTR_MASK;

		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_SPK_MIX_DCTL_MASK | EN_SPK_MIX_DCTR_MASK, 0);

		spk_gain = snd_soc_read(w->codec, COD3022X_32_VOL_EP_SPK);

		snd_soc_update_bits(w->codec, COD3022X_32_VOL_EP_SPK,
				CTVOL_SPK_PGA_MASK,
				0x3 << CTVOL_SPK_PGA_SHIFT);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				PW_AUTO_DA_MASK | APW_SPK_MASK,
				PW_AUTO_DA_MASK | APW_SPK_MASK);

		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_SPK_MIX_DCTL_MASK | EN_SPK_MIX_DCTR_MASK |
				EN_SPK_MIX_MIXL_MASK | EN_SPK_MIX_MIXR_MASK,
				mix_val);

		msleep(135);

		snd_soc_update_bits(w->codec, COD3022X_32_VOL_EP_SPK,
				CTVOL_SPK_PGA_MASK, spk_gain);

		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec, COD3022X_32_VOL_EP_SPK,
				CTVOL_SPK_PGA_MASK, 0x3);

		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_SPK_MIX_DCTL_MASK | EN_SPK_MIX_DCTR_MASK, 0);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				APW_SPK_MASK, 0);

		cod3022x_usleep(500);

		/* Check HP is ON */
		hp_on = snd_soc_read(w->codec, COD3022X_76_CHOP_DA);
		if ((hp_on & EN_HP_CHOP_MASK)) {
			/* We are in HP only mode */

			snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x18);
			snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x18);
			msleep(6);

			/* enable DNC */
			snd_soc_update_bits(w->codec,
				COD3022X_54_DNC1,EN_DNC_MASK, EN_DNC_MASK);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int hpdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int hp_on, spk_on, ep_on;
	int chop_val;
	unsigned int detb_period = CTMF_DETB_PERIOD_2048;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(w->codec);

	if (cod3022x->use_external_jd || cod3022x->use_btn_adc_mode)
		detb_period = CTMF_DETB_PERIOD_8;

	chop_val = snd_soc_read(w->codec, COD3022X_76_CHOP_DA);
	hp_on = chop_val & EN_HP_CHOP_MASK;
	spk_on = chop_val & EN_SPK_PGA_CHOP_MASK;
	ep_on = chop_val & EN_EP_CHOP_MASK;

	if (!hp_on) {
		dev_dbg(w->codec->dev, "%s called but headphone not enabled\n",
				__func__);
		return 0;
	}

	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022x_hp_playback_init(w->codec);
		break;

	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 0x86 register set to 0x0f (Always-On) for all modes
		 */
		snd_soc_update_bits(w->codec, COD3022X_86_DET_TIME,
					CTMF_DETB_PERIOD_MASK, 0x0);

		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				PW_AUTO_DA_MASK | APW_HP_MASK,
				PW_AUTO_DA_MASK | APW_HP_MASK);
		msleep(160);

		snd_soc_update_bits(w->codec, COD3022X_D7_CTRL_CP1,
			CTRV_CP_NEGREF_MASK, 0x04);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK, 0);
		cod3022x_usleep(100);

		snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x1E);
		snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x1E);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK, EN_HP_SV_MASK);
		cod3022x_usleep(100);

		if (!spk_on && !ep_on) {
			/* Only HP is on, enable DNC and set default analog HP
			 * volume
			 */
			snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x18);
			snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x18);
			msleep(6);

			/* Limiter level selection -0.2dB (defult) */
			snd_soc_update_bits(w->codec, COD3022X_54_DNC1,
					EN_DNC_MASK, EN_DNC_MASK);
		} else {
			/* Either SPK or EP is on, disable DNC and set given
			 * analog HP volume
			 */
			snd_soc_write(w->codec, COD3022X_30_VOL_HPL,
							cod3022x->vol_hpl);
			snd_soc_write(w->codec, COD3022X_31_VOL_HPR,
							cod3022x->vol_hpr);
		}

		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec, COD3022X_54_DNC1,
				EN_DNC_MASK , 0);
		cod3022x_usleep(100);

		snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x1E);
		snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x1E);
		msleep(6);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK, 0);
		cod3022x_usleep(100);

		snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x26);
		snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x26);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK,
				EN_HP_SV_MASK);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_D7_CTRL_CP1,
			CTRV_CP_NEGREF_MASK, 0x0f);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				APW_HP_MASK, 0);
		msleep(40);

		snd_soc_update_bits(w->codec, COD3022X_36_MIX_DA1,
				EN_HP_MIXL_DCTL_MASK | EN_HP_MIXR_DCTR_MASK, 0);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_86_DET_TIME,
				CTMF_DETB_PERIOD_MASK,
				(detb_period << CTMF_DETB_PERIOD_SHIFT));

		dev_dbg(w->codec->dev, "codec DET time reg value=0x%02x @ %s\n",
				snd_soc_read(w->codec, COD3022X_86_DET_TIME),
				__func__);

		cod3022x_usleep(100);

		break;

	default:
		break;
	}

	return 0;
}

static int epdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	unsigned int ep_on;
	unsigned int mix_val;

	ep_on = snd_soc_read(w->codec, COD3022X_76_CHOP_DA);
	if (!(ep_on & EN_EP_CHOP_MASK)) {
		dev_dbg(w->codec->dev, "%s called but ear-piece not enabled\n",
				__func__);
		return 0;
	}
	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Update OTP configuration */
		cod3022x_update_playback_otp(w->codec);
		/* enable soft mute */
		snd_soc_update_bits(w->codec, COD3022X_50_DAC1,
			DAC1_SOFT_MUTE_MASK, DAC1_SOFT_MUTE_MASK);

		snd_soc_update_bits(w->codec, COD3022X_D7_CTRL_CP1,
			CTRV_CP_NEGREF_MASK, 0x00);

		snd_soc_update_bits(w->codec, COD3022X_10_PD_REF,
					PDB_VMID_MASK, PDB_VMID_MASK);

		snd_soc_update_bits(w->codec, COD3022X_13_PD_DA1,
					EN_DCTL_PREQ_MASK | EN_DCTR_PREQ_MASK,
					EN_DCTL_PREQ_MASK | EN_DCTR_PREQ_MASK);

		snd_soc_update_bits(w->codec, COD3022X_10_PD_REF,
					PDB_IGEN_MASK, PDB_IGEN_MASK);

		snd_soc_update_bits(w->codec, COD3022X_13_PD_DA1,
					PDB_DCTL_MASK, PDB_DCTL_MASK);

		snd_soc_update_bits(w->codec, COD3022X_13_PD_DA1,
					EN_DCTL_PREQ_MASK |
					EN_DCTR_PREQ_MASK, 0);

		snd_soc_update_bits(w->codec, COD3022X_13_PD_DA1,
					RESETB_DCTL_MASK, RESETB_DCTL_MASK);

		snd_soc_update_bits(w->codec, COD3022X_DC_CTRL_EPS,
					CTMI_EP_A_MASK, CTMI_EP_A_MASK);

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_EP_CORE_MASK, PDB_EP_CORE_MASK);

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_EP_DRV_MASK, PDB_EP_DRV_MASK);
		msleep(50);

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_EP_CORE_MASK, 0x00);

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_EP_DRV_MASK, 0x00);

		snd_soc_update_bits(w->codec, COD3022X_DC_CTRL_EPS,
				CTMI_EP_A_MASK,
				(CTMI_EP_A_2_UA << CTMI_EP_A_SHIFT));

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_DOUBLER_MASK | PDB_CP_MASK,
					PDB_DOUBLER_MASK | PDB_CP_MASK);
		msleep(1);

		snd_soc_update_bits(w->codec, COD3022X_D7_CTRL_CP1,
			CTRV_CP_NEGREF_MASK, 0x04);

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_EP_CORE_MASK, PDB_EP_CORE_MASK);

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_EP_DRV_MASK, PDB_EP_DRV_MASK);

		snd_soc_update_bits(w->codec, COD3022X_33_CTRL_EP,
					EN_EP_PRT_MASK | EN_EP_IDET_MASK,
					EN_EP_PRT_MASK | EN_EP_IDET_MASK);

		mix_val = snd_soc_read(w->codec, COD3022X_37_MIX_DA2);
		mix_val &= EN_EP_MIX_DCTL_MASK | EN_EP_MIX_DCTR_MASK |
			EN_EP_MIX_MIXL_MASK | EN_EP_MIX_MIXR_MASK;

		/* Enable DAC path by default */
		mix_val |= EN_EP_MIX_DCTL_MASK;

		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_EP_MIX_DCTL_MASK | EN_EP_MIX_DCTR_MASK |
				EN_EP_MIX_MIXL_MASK | EN_EP_MIX_MIXR_MASK,
				mix_val);

		cod3022x_usleep(100);
		/* disable_soft_mute */
		snd_soc_update_bits(w->codec, COD3022X_50_DAC1,
			DAC1_SOFT_MUTE_MASK, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* enable soft mute */
		snd_soc_update_bits(w->codec, COD3022X_50_DAC1,
			DAC1_SOFT_MUTE_MASK, DAC1_SOFT_MUTE_MASK);
		msleep(24);

		snd_soc_update_bits(w->codec, COD3022X_D8_CTRL_CP2,
			CTMD_CP_H2L_MASK,
			VIDD_HALF_VDD_DELAY_32MS << CTMD_CP_H2L_SHIFT);

		snd_soc_update_bits(w->codec, COD3022X_D7_CTRL_CP1,
			CTRV_CP_NEGREF_MASK, 0x0f);

		snd_soc_update_bits(w->codec, COD3022X_33_CTRL_EP,
					CTMV_CP_MODE_MASK,
					CTMV_CP_MODE_HALF_VDD << CTMV_CP_MODE_SHIFT);

		msleep(150);
		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_EP_MIX_DCTL_MASK | EN_EP_MIX_DCTR_MASK, 0x0);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_33_CTRL_EP,
					EN_EP_PRT_MASK | EN_EP_IDET_MASK, 0);

		snd_soc_update_bits(w->codec, COD3022X_15_PD_DA3,
					PDB_DOUBLER_MASK | PDB_CP_MASK |
					PDB_EP_CORE_MASK |
					PDB_EP_DRV_MASK, 0);

		snd_soc_update_bits(w->codec, COD3022X_13_PD_DA1,
						PDB_DCTL_MASK |
						RESETB_DCTL_MASK, 0);

		snd_soc_update_bits(w->codec, COD3022X_10_PD_REF,
					PDB_VMID_MASK | PDB_IGEN_MASK, 0);

		snd_soc_update_bits(w->codec, COD3022X_33_CTRL_EP,
					CTMV_CP_MODE_MASK,
					CTMV_CP_MODE_ANALOG << CTMV_CP_MODE_SHIFT);

		snd_soc_update_bits(w->codec, COD3022X_D8_CTRL_CP2,
				CTMD_CP_H2L_MASK,
				VIDD_HALF_VDD_DELAY_64MS << CTMD_CP_H2L_SHIFT);

		/* disable_soft_mute */
		snd_soc_update_bits(w->codec, COD3022X_50_DAC1,
			DAC1_SOFT_MUTE_MASK, 0);
	default:
		break;
	}

	return 0;
}

static int mic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int mic_on;

	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	mic_on = snd_soc_read(w->codec, COD3022X_75_CHOP_AD);
	if (!(mic_on & EN_MCB2_CHOP_MASK)) {
		dev_dbg(w->codec->dev, "%s: MIC2 is not enabled, returning.\n",
								__func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022x_adc_digital_mute(w->codec, true);
		cod3022_power_on_mic2(w->codec);
		cod3022x_adc_digital_mute(w->codec, false);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod3022x_adc_digital_mute(w->codec, true);
		cod3022_power_off_mic2(w->codec);
		cod3022x_adc_digital_mute(w->codec, false);
		break;
	default:
		break;
	}

	return 0;
}

static int mic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int mic_on;

	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	mic_on = snd_soc_read(w->codec, COD3022X_75_CHOP_AD);
	if (!(mic_on & EN_MCB1_CHOP_MASK)) {
		dev_dbg(w->codec->dev, "%s: MIC1 is not enabled, returning.\n",
								__func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022x_adc_digital_mute(w->codec, true);
		cod3022_power_on_mic1(w->codec);
		cod3022x_adc_digital_mute(w->codec, false);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod3022x_adc_digital_mute(w->codec, true);
		cod3022_power_off_mic1(w->codec);
		cod3022x_adc_digital_mute(w->codec, false);
		break;

	default:
		break;
	}

	return 0;
}

static int linein_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int linein_on;

	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	linein_on = snd_soc_read(w->codec, COD3022X_75_CHOP_AD);
	if (!(linein_on & EN_LN_CHOP_MASK)) {
		dev_dbg(w->codec->dev, "%s: LINE IN not enabled, returning.\n",
								__func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022x_adc_digital_mute(w->codec, true);
		cod3022_power_on_linein(w->codec);
		cod3022x_adc_digital_mute(w->codec, false);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod3022x_adc_digital_mute(w->codec, true);
		cod3022_power_off_linein(w->codec);
		cod3022x_adc_digital_mute(w->codec, false);
		break;

	default:
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new hpl_mix[] = {
	SOC_DAPM_SINGLE("DACL Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXL_DCTL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DACR Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXL_DCTR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCL Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXL_MIXL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCR Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXL_MIXR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hpr_mix[] = {
	SOC_DAPM_SINGLE("DACL Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXR_DCTL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DACR Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXR_DCTR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCL Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXR_MIXL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCR Switch", COD3022X_36_MIX_DA1,
			EN_HP_MIXR_MIXR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new ep_mix[] = {
	SOC_DAPM_SINGLE("DACL Switch", COD3022X_37_MIX_DA2,
			EN_EP_MIX_DCTL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DACR Switch", COD3022X_37_MIX_DA2,
			EN_EP_MIX_DCTR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCL Switch", COD3022X_37_MIX_DA2,
			EN_EP_MIX_MIXL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCR Switch", COD3022X_37_MIX_DA2,
			EN_EP_MIX_MIXR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new spk_mix[] = {
	SOC_DAPM_SINGLE("DACL Switch", COD3022X_37_MIX_DA2,
			EN_SPK_MIX_DCTL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DACR Switch", COD3022X_37_MIX_DA2,
			EN_SPK_MIX_DCTR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCL Switch", COD3022X_37_MIX_DA2,
			EN_SPK_MIX_MIXL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("ADCR Switch", COD3022X_37_MIX_DA2,
			EN_SPK_MIX_MIXR_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new spk_on[] = {
	SOC_DAPM_SINGLE("SPK On", COD3022X_76_CHOP_DA,
				EN_SPK_PGA_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hp_on[] = {
	SOC_DAPM_SINGLE("HP On", COD3022X_76_CHOP_DA, EN_HP_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new ep_on[] = {
	SOC_DAPM_SINGLE("EP On", COD3022X_76_CHOP_DA, EN_EP_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new mic1_on[] = {
	SOC_DAPM_SINGLE("MIC1 On", COD3022X_75_CHOP_AD,
					EN_MCB1_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new mic2_on[] = {
	SOC_DAPM_SINGLE("MIC2 On", COD3022X_75_CHOP_AD,
					EN_MCB2_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new linein_on[] = {
	SOC_DAPM_SINGLE("LINEIN On", COD3022X_75_CHOP_AD,
					EN_LN_CHOP_SHIFT, 1, 0),
};

static const struct snd_soc_dapm_widget cod3022x_dapm_widgets[] = {
	SND_SOC_DAPM_SWITCH("SPK", SND_SOC_NOPM, 0, 0, spk_on),
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM, 0, 0, hp_on),
	SND_SOC_DAPM_SWITCH("EP", SND_SOC_NOPM, 0, 0, ep_on),
	SND_SOC_DAPM_SWITCH("MIC1", SND_SOC_NOPM, 0, 0, mic1_on),
	SND_SOC_DAPM_SWITCH("MIC2", SND_SOC_NOPM, 0, 0, mic2_on),
	SND_SOC_DAPM_SWITCH("LINEIN", SND_SOC_NOPM, 0, 0, linein_on),

	SND_SOC_DAPM_SUPPLY("VMID", SND_SOC_NOPM, 0, 0, vmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUT_DRV_E("SPKDRV", SND_SOC_NOPM, 0, 0, NULL, 0,
			spkdrv_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUT_DRV_E("EPDRV", SND_SOC_NOPM, 0, 0, NULL, 0,
			epdrv_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0, 0, NULL, 0,
			hpdrv_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("MIC1_PGA", SND_SOC_NOPM, 0, 0,
			NULL, 0, mic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("MIC2_PGA", SND_SOC_NOPM, 0, 0,
			NULL, 0, mic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("LINEIN_PGA", SND_SOC_NOPM, 0, 0,
			NULL, 0, linein_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("HPL Mixer", SND_SOC_NOPM, 0, 0, hpl_mix,
			ARRAY_SIZE(hpl_mix)),
	SND_SOC_DAPM_MIXER("HPR Mixer", SND_SOC_NOPM, 0, 0, hpr_mix,
			ARRAY_SIZE(hpr_mix)),
	SND_SOC_DAPM_MIXER("EP Mixer", SND_SOC_NOPM, 0, 0, ep_mix,
			ARRAY_SIZE(ep_mix)),
	SND_SOC_DAPM_MIXER("SPK Mixer", SND_SOC_NOPM, 0, 0, spk_mix,
			ARRAY_SIZE(spk_mix)),

	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0,
			dac_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_DAC_E("DAC", "AIF2 Playback", SND_SOC_NOPM, 0, 0,
			dac_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_ADC_E("ADC", "AIF Capture", SND_SOC_NOPM, 0, 0,
			adc_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "AIF2 Capture", SND_SOC_NOPM, 0, 0,
			adc_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
	SND_SOC_DAPM_OUTPUT("HPOUTLN"),
	SND_SOC_DAPM_OUTPUT("EPOUTN"),
	SND_SOC_DAPM_OUTPUT("AIF4OUT"),

	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("IN3L"),

	SND_SOC_DAPM_INPUT("AIF4IN"),
};

static const struct snd_soc_dapm_route cod3022x_dapm_routes[] = {
	/* Sink, Control, Source */
	{"SPK Mixer", "ADCL Switch", "ADCL Mixer"},
	{"SPK Mixer", "ADCR Switch", "ADCR Mixer"},
	{"SPK Mixer", "DACL Switch", "DAC"},
	{"SPK Mixer", "DACR Switch", "DAC"},

	{"SPKDRV", NULL, "SPK Mixer"},
	{"SPKDRV", NULL, "DAC"},
	{"SPK" , "SPK On", "SPKDRV"},
	{"SPKOUTLN", NULL, "SPK"},

	{"EP Mixer", "ADCL Switch", "ADCL Mixer"},
	{"EP Mixer", "ADCR Switch", "ADCR Mixer"},
	{"EP Mixer", "DACL Switch", "DAC"},
	{"EP Mixer", "DACR Switch", "DAC"},

	{"EPDRV", NULL, "EP Mixer"},
	{"EPDRV", NULL, "DAC"},
	{"EP", "EP On", "EPDRV"},
	{"EPOUTN", NULL, "EP"},

	{"HPL Mixer", "ADCL Switch", "ADCL Mixer"},
	{"HPL Mixer", "ADCR Switch", "ADCR Mixer"},
	{"HPL Mixer", "DACL Switch", "DAC"},
	{"HPL Mixer", "DACR Switch", "DAC"},

	{"HPR Mixer", "ADCL Switch", "ADCL Mixer"},
	{"HPR Mixer", "ADCR Switch", "ADCR Mixer"},
	{"HPR Mixer", "DACL Switch", "DAC"},
	{"HPR Mixer", "DACR Switch", "DAC"},

	{"HPDRV", NULL, "HPL Mixer"},
	{"HPDRV", NULL, "HPR Mixer"},
	{"HPDRV", NULL, "DAC"},
	{"HP", "HP On", "HPDRV"},
	{"HPOUTLN", NULL, "HP"},

	{"DAC" , NULL, "AIF Playback"},
	{"DAC" , NULL, "AIF2 Playback"},

	{"MIC1_PGA", NULL, "IN1L"},
	{"MIC1_PGA", NULL, "VMID"},
	{"MIC1", "MIC1 On", "MIC1_PGA"},

	{"ADC", "NULL", "MIC1"},

	{"MIC2_PGA", NULL, "IN2L"},
	{"MIC2_PGA", NULL, "VMID"},
	{"MIC2", "MIC2 On", "MIC2_PGA"},

	{"ADC", "NULL", "MIC2"},

	{"LINEIN_PGA", NULL, "IN3L"},
	{"LINEIN_PGA", NULL, "VMID"},
	{"LINEIN", "LINEIN On", "LINEIN_PGA"},

	{"ADC", "NULL", "LINEIN"},

	{"ADC", NULL, "ADCL Mixer"},
	{"ADC", NULL, "ADCR Mixer"},

	{"AIF Capture", NULL, "ADC"},
	{"AIF2 Capture", NULL, "ADC"},
};

static int cod3022x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int bclk = 0, lrclk = 0;

	dev_dbg(codec->dev, "%s called\n", __func__);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
		fmt = LRJ_AUDIO_FORMAT_MASK;
		break;

	case SND_SOC_DAIFMT_I2S:
		fmt = I2S_AUDIO_FORMAT_MASK;
		break;

	default:
		pr_err("Unsupported DAI format %d\n",
				fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, COD3022X_41_FORMAT,
			I2S_AUDIO_FORMAT_MASK | LRJ_AUDIO_FORMAT_MASK, fmt);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		bclk = BCLK_POL_MASK;
		lrclk = LRCLK_POL_MASK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		bclk = BCLK_POL_MASK;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		lrclk = LRCLK_POL_MASK;
		break;
	default:
		pr_err("Unsupported Polartiy selection %d\n",
				fmt & SND_SOC_DAIFMT_INV_MASK);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, COD3022X_41_FORMAT,
			BCLK_POL_MASK | LRCLK_POL_MASK, bclk | lrclk);
	return 0;
}

int cod3022x_set_externel_jd(struct snd_soc_codec *codec)
{
	int ret;
	struct cod3022x_priv *cod3022x;

	if (codec == NULL) {
		pr_err("Initilaise codec, before calling %s\n", __func__);
		return -1;
	}

	dev_dbg(codec->dev, "%s called\n", __func__);

	cod3022x = snd_soc_codec_get_drvdata(codec);

	cod3022x->use_external_jd = true;

	pm_runtime_get_sync(codec->dev);

	/* Mask all interrupt bits for external jd mode */
	snd_soc_update_bits(codec, COD3022X_05_IRQ1M, IRQ1M_MASK_ALL, 0xff);
	snd_soc_update_bits(codec, COD3022X_06_IRQ2M, IRQ2M_MASK_ALL, 0xff);

	/* Enable External jack detecter */
	ret = snd_soc_update_bits(codec, COD3022X_83_JACK_DET1,
			CTMP_JD_MODE_MASK, CTMP_JD_MODE_MASK);

	/* Disable Internel Jack detecter */
	ret |= snd_soc_update_bits(codec, COD3022X_81_DET_ON,
			EN_PDB_JD_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_86_DET_TIME,
			CTMD_BTN_DBNC_MASK | CTMF_BTN_ON_MASK |
			CTMF_DETB_PERIOD_MASK,
			((CTMD_BTN_DBNC_5 << CTMD_BTN_DBNC_SHIFT) |
			(CTMF_BTN_ON_14_CLK << CTMF_BTN_ON_SHIFT) |
			(CTMF_DETB_PERIOD_8 << CTMF_DETB_PERIOD_SHIFT)));

	dev_dbg(codec->dev, "codec DET time reg value=0x%02x @ %s\n",
			snd_soc_read(codec, COD3022X_86_DET_TIME), __func__);

	pm_runtime_put_sync(codec->dev);

	return ret;
}

static int cod3022x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

static void cod3022x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);
}

static void cod3022x_sys_reset(struct snd_soc_codec *codec)
{
	unsigned char otp_addr[COD3022X_RESTORE_OTP_COUNT] = {
					0xd4, 0xd5, 0xd6, 0xdb, 0xdc
					};

	unsigned char reg_addr[COD3022X_RESTORE_REG_COUNT]= {
					0x20, 0x22, 0x30, 0x31, 0x32,
					0x36, 0X37, 0x42, 0x44, 0X71,
					0x75, 0x5a, 0x54, 0x40, 0x16,
					0x17};

	unsigned char otp_val[COD3022X_RESTORE_OTP_COUNT];
	unsigned char reg_val[COD3022X_RESTORE_REG_COUNT];
	unsigned int i;

	dev_dbg(codec->dev, "%s called\n", __func__);

	/* TODO: Check if we can use cod3022x_{restore/save}_otp_registers() */
	/* OTP register values are read from 0xF* and written to 0xD* */
	for(i = 0; i < COD3022X_RESTORE_OTP_COUNT; i++)
		otp_val[i] = snd_soc_read(codec,
				(otp_addr[i] + COD3022X_OTP_R_OFFSET));

	for(i = 0; i < COD3022X_RESTORE_REG_COUNT; i++)
		reg_val[i] = snd_soc_read(codec, reg_addr[i]);

	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
						SYS_RSTB_MASK, 0);
	mdelay(1);
	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
					SYS_RSTB_MASK, SYS_RSTB_MASK);
	mdelay(1);

	for(i = 0; i < COD3022X_RESTORE_OTP_COUNT; i++)
		snd_soc_write(codec, otp_addr[i], otp_val[i]);

	for(i = 0; i < COD3022X_RESTORE_REG_COUNT; i++)
		snd_soc_write(codec, reg_addr[i], reg_val[i]);
}

static int cod3022x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	unsigned int cur_aifrate;
	int dnc;
	int ret;

	dev_dbg(codec->dev, "%s called for aif (%s)\n", __func__,
					substream->stream ? "C" : "P");

	/* 192 KHz support */
	cur_aifrate = params_rate(params);
	if (cod3022x->aifrate != cur_aifrate) {
		/* DNC needs to be disabled while switching samplerate */
		dnc = snd_soc_read(codec, COD3022X_54_DNC1);
		snd_soc_write(codec, COD3022X_54_DNC1, 0);

		/* Need to reset H/W while switching from 192KHz to 48KHz */
		if (cur_aifrate == COD3022X_SAMPLE_RATE_192KHZ) {
			snd_soc_update_bits(codec, COD3022X_53_MQS,
					MQS_MODE_MASK, MQS_MODE_MASK);
		} else if (cod3022x->aifrate == COD3022X_SAMPLE_RATE_192KHZ) {
			snd_soc_update_bits(codec, COD3022X_53_MQS,
					MQS_MODE_MASK, 0);
			cod3022x_sys_reset(codec);
		}

		/* DNC mode can be restored after the samplerate switch */
		snd_soc_write(codec, COD3022X_54_DNC1, dnc);
		cod3022x->aifrate = cur_aifrate;
	}

	/*
	 * Codec supports only 24bits per sample, Mixer performs the required
	 * conversion to 24 bits. BFS is fixed at 64fs for mixer<->codec
	 * interface.
	 */
	ret = snd_soc_update_bits(codec, COD3022X_41_FORMAT,
			DATA_WORD_LENGTH_MASK,
			(DATA_WORD_LENGTH_24 << DATA_WORD_LENGTH_SHIFT));
	if (ret < 0) {
		dev_err(codec->dev, "%s failed to set bits per sample\n",
				__func__);
		return ret;
	}

	return 0;
}

static void cod3022x_jack_det_work(struct work_struct *work)
{
	struct cod3022x_priv *cod3022x =
		container_of(work, struct cod3022x_priv, jack_det_work.work);
	struct cod3022x_jack_det *jackdet = &cod3022x->jack_det;
	unsigned int detb_period = cod3022x->use_btn_adc_mode ?
				CTMF_DETB_PERIOD_8 : CTMF_DETB_PERIOD_2048;
	struct snd_soc_codec *codec = cod3022x->codec;
	int adc;

	mutex_lock(&cod3022x->jackdet_lock);

	if(jackdet->jack_det == true){
	/* read adc for mic detect */
		adc = cod3022x_adc_get_value(cod3022x);
		dev_dbg(cod3022x->dev, " %s mic det adc  %d \n" , __func__, adc);

		if ( adc > cod3022x->mic_adc_range )
			jackdet->mic_det = true;
		else
			jackdet->mic_det = false;
	} else {
		/* jack/mic out */
		jackdet->mic_det = false;
	}

	if (jackdet->button_det == true){
		if (!jackdet->jack_det && !jackdet->mic_det) {
			jackdet->button_det = false;
			input_report_key(cod3022x->input, jackdet->button_code, 0);
			input_sync(cod3022x->input);
			cod3022x_process_button_ev(jackdet->button_code, 0);
			dev_dbg(cod3022x->dev, ":key %d released\n", jackdet->button_code);
		}
	}

	if (jackdet->jack_det && jackdet->mic_det)
		switch_set_state(&cod3022x->sdev, 1);
	else if (jackdet->jack_det)
		switch_set_state(&cod3022x->sdev, 2);
	else
		switch_set_state(&cod3022x->sdev, 0);

	if (cod3022x->is_suspend)
		regcache_cache_only(cod3022x->regmap, false);

	/*
	 * Using codec internal jack detection, there is some noise issue.
	 *  So , 0x86 detection time set to 0xff when insert 3pole jack.
	 */
	if (!jackdet->jack_det && !jackdet->mic_det) {
		snd_soc_update_bits(codec, COD3022X_86_DET_TIME,
			CTMF_DETB_PERIOD_MASK,
			(detb_period << CTMF_DETB_PERIOD_SHIFT));
		/* Set Jack debounce time Min when remove 3pole and 4pole jack. */
		snd_soc_update_bits(codec, COD3022X_84_JACK_DET2,
				CTMD_JD_DBNC_MASK, CTMD_JD_DBNC_MASK);
		snd_soc_update_bits(codec, COD3022X_87_LDO_DIG,
				CTMD_JD_DBNC_4_MASK, CTMD_JD_DBNC_4_MASK);
	} else {
		/* Set Jack debounce time Max when insert 3pole and 4pole jack. */
		snd_soc_update_bits(codec, COD3022X_84_JACK_DET2,
				CTMD_JD_DBNC_MASK, 0x0);
		snd_soc_update_bits(codec, COD3022X_87_LDO_DIG,
				CTMD_JD_DBNC_4_MASK, 0x0);

		 /* 0x86 register set to 0x0f (Always-On) for all modes */
		snd_soc_update_bits(codec, COD3022X_86_DET_TIME,
						CTMF_DETB_PERIOD_MASK, 0x00);
	}

	dev_dbg(codec->dev, "codec DET time reg value=0x%02x @ %s\n",
			snd_soc_read(codec, COD3022X_86_DET_TIME), __func__);

	if (cod3022x->is_suspend)
		regcache_cache_only(cod3022x->regmap, true);

	dev_dbg(cod3022x->codec->dev, "Jack %s, Mic %s\n",
				jackdet->jack_det ? "inserted" : "removed",
				jackdet->mic_det ? "inserted" : "removed");

	mutex_unlock(&cod3022x->jackdet_lock);
}

#define ADC_TRACE_NUM		5
#define ADC_TRACE_NUM2		2
#define ADC_READ_DELAY_US	500
#define ADC_READ_DELAY_MS	1
#define ADC_DEVI_THRESHOLD	18000

#define BUTTON_PRESS 1
#define BUTTON_RELEASE 0

static int get_adc_avg(int* adc_values)
{
	int i;
	int adc_sum=0;
	for ( i=0; i<ADC_TRACE_NUM; i++) {
		adc_sum += adc_values[i];
	}
	adc_sum = adc_sum / ADC_TRACE_NUM;
	return adc_sum;
}

static int get_adc_devi(int avg , int* adc_values)
{
	int i;
	int devi=0, diff;
	for ( i=0; i<ADC_TRACE_NUM; i++) {
		diff = adc_values[i]-avg;
		devi += (diff*diff);
	}
	return devi;
}

static void cod3022x_buttons_work(struct work_struct *work)
{
	struct cod3022x_priv *cod3022x =
		container_of(work, struct cod3022x_priv, buttons_work.work);
	struct cod3022x_jack_det *jd = &cod3022x->jack_det;
	struct jack_buttons_zone *btn_zones = cod3022x->jack_buttons_zones;
	int num_buttons_zones = ARRAY_SIZE(cod3022x->jack_buttons_zones);
	int adc_values[ADC_TRACE_NUM];
	int current_button_state;
	int adc;
	int i, avg, devi;
	int adc_final_values[ADC_TRACE_NUM2];
	int j;
	int adc_final = 0;
	int adc_max = 0;

	if (!jd->jack_det) {
		dev_err(cod3022x->dev, "Skip button events for jack_out\n");
		return;
	}
	if (!jd->mic_det) {
		dev_err(cod3022x->dev, "Skip button events for 3-pole jack\n");
		return;
	}

	for ( j=0; j<ADC_TRACE_NUM2; j++) {
		/* read GPADC for button */
		for ( i=0; i<ADC_TRACE_NUM; i++) {
			adc = cod3022x_adc_get_value(cod3022x);
			adc_values[i] = adc;
			udelay(ADC_READ_DELAY_US);
		}

		/*
		 * check avg/devi value is proper
		 * if not read adc after 5 ms
		 */
		avg = get_adc_avg(adc_values);
		devi = get_adc_devi(avg,adc_values);
		dev_dbg(cod3022x->dev, ":button adc avg: %d, devi: %d\n",avg, devi);

		if (devi > ADC_DEVI_THRESHOLD ) {
			queue_delayed_work(cod3022x->buttons_wq,
					&cod3022x->buttons_work, 5);
			for ( i=0; i<ADC_TRACE_NUM; ){
				dev_dbg(cod3022x->dev, ":retry button_work :  %d %d %d %d %d\n",
				adc_values[i+ 0], adc_values[i+ 1], adc_values[i+ 2], adc_values[i+ 3], adc_values[i+ 4]);
				i += 5;
			}
			return;
		}
		adc_final_values[j] = avg;

		if (avg > adc_max)
			adc_max = avg;
		mdelay(ADC_READ_DELAY_MS);
	}
	adc_final = adc_max;

	/* check button press/release */
	if(adc_final > cod3022x->btn_release_value)
		current_button_state = BUTTON_RELEASE;
	else
		current_button_state = BUTTON_PRESS;

	if( jd->privious_button_state == current_button_state) {
		return;
	}

	jd->privious_button_state = current_button_state;

	adc = adc_final;
	jd->adc_val = adc_final;
	/* determine which button press or release */
	if(current_button_state == BUTTON_PRESS) {
		for (i = 0; i < num_buttons_zones; i++)
			if (adc >= btn_zones[i].adc_low &&
				adc <= btn_zones[i].adc_high) {
				jd->button_code = btn_zones[i].code;
				input_report_key(cod3022x->input, jd->button_code, 1);
				input_sync(cod3022x->input);
				jd->button_det = true;
				cod3022x_process_button_ev(jd->button_code, 1);
				dev_dbg(cod3022x->dev, ":key %d is pressed, adc %d\n",
						 btn_zones[i].code, adc);
				return;
		}

		dev_err(cod3022x->dev, ":key skipped. ADC %d\n", adc);
	} else {
		jd->button_det = false;
		input_report_key(cod3022x->input, jd->button_code, 0);
		input_sync(cod3022x->input);
		cod3022x_process_button_ev(jd->button_code, 0);
		dev_dbg(cod3022x->dev, ":key %d released\n", jd->button_code);
	}

	return;
}

static irqreturn_t cod3022x_threaded_isr(int irq, void *data)
{
	struct cod3022x_priv *cod3022x = data;
	struct snd_soc_codec *codec = cod3022x->codec;
	struct cod3022x_jack_det *jd = &cod3022x->jack_det;
	unsigned int stat1, pend1, pend2;
	int jackdet = COD3022X_MJ_DET_INVALID;
	bool det_status_change = false;
	unsigned int i, key;

	mutex_lock(&cod3022x->key_lock);

	if (cod3022x->is_suspend)
		regcache_cache_only(cod3022x->regmap, false);

	pend1 = snd_soc_read(codec, COD3022X_01_IRQ1PEND);
	pend2 = snd_soc_read(codec, COD3022X_02_IRQ2PEND);
	stat1 = snd_soc_read(cod3022x->codec, COD3022X_09_STATUS1);

	dev_dbg(codec->dev, "Pend1=0x%02x, Pend2=0x%02x, Stat1=0x%02x\n",
							pend1, pend2, stat1);

	/*
	 * Sequence for Jack/Mic detection
	 *
	 * (JACK bit 0, MIC bit 1)
	 *
	 * 1. Check bits in IRQ1PEND and IRQ2PEND.
	 * 2. If either of them is 1, then the STATUS1 register tells current
	 * status of Jack/Mic. Connected if bit value is 1, removed otherwise.
	 */
	if ((pend1 & IRQ1_JACK_DET_R) || (pend2 & IRQ2_JACK_DET_F)) {
		det_status_change = true;
		jackdet = stat1 & BIT(STATUS1_JACK_DET_SHIFT);
		jd->jack_det = jackdet ? true : false;
	}


	if (det_status_change) {
		/* mic detection delay */
		queue_delayed_work(cod3022x->jack_det_wq,
			&cod3022x->jack_det_work, cod3022x->mic_det_delay);
		mutex_unlock(&cod3022x->key_lock);
		goto out;
	}

	if (cod3022x->use_btn_adc_mode) {
		/* start button work */
		queue_delayed_work(cod3022x->buttons_wq,
					&cod3022x->buttons_work, 5);
	} else {
		for (i = COD3022X_START_IRQ_CHK_BIT;
				i <= COD3022X_MAX_IRQ_CHK_BITS; i++) {
			key = pend2 & BIT(i);

			switch (key) {
			case IRQ2_HOOK_DET_R:
				input_event(cod3022x->input, EV_KEY,
						KEY_MEDIA, 1);
				jd->button_det = true;
				dev_dbg(codec->dev, "hook pushed\n");
				break;

			case IRQ2_HOOK_DET_F:
				input_event(cod3022x->input, EV_KEY,
						KEY_MEDIA, 0);
				jd->button_det = false;
				dev_dbg(codec->dev, "hook released\n");
				break;

			case IRQ2_VOL_UP_DET_R:
				input_event(cod3022x->input, EV_KEY,
						KEY_VOLUMEUP, 1);
				jd->button_det = true;
				dev_dbg(codec->dev, "vol+ pushed\n");
				break;

			case IRQ2_VOL_UP_DET_F:
				input_event(cod3022x->input, EV_KEY,
						KEY_VOLUMEUP, 0);
				jd->button_det = false;
				dev_dbg(codec->dev, "vol+ released\n");
				break;

			case IRQ2_VOL_DN_DET_R:
				input_event(cod3022x->input, EV_KEY,
						KEY_VOLUMEDOWN, 1);
				jd->button_det = true;
				dev_dbg(codec->dev, "vol- pushed\n");
				break;

			case IRQ2_VOL_DN_DET_F:
				input_event(cod3022x->input, EV_KEY,
						KEY_VOLUMEDOWN, 0);
				jd->button_det = false;
				dev_dbg(codec->dev, "vol- released\n");
				break;

			default:
				break;
			}
		}
		input_sync(cod3022x->input);
	}

	mutex_unlock(&cod3022x->key_lock);

out:
	if (cod3022x->is_suspend)
		regcache_cache_only(cod3022x->regmap, true);

	return IRQ_HANDLED;
}

int cod3022x_jack_mic_register(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	unsigned int detb_period = cod3022x->use_btn_adc_mode ?
				CTMF_DETB_PERIOD_8 : CTMF_DETB_PERIOD_2048;
	int num_buttons_zones = ARRAY_SIZE(cod3022x->jack_buttons_zones);
	int i, ret;


	cod3022x->sdev.name = "h2w";

	ret = switch_dev_register(&cod3022x->sdev);
	if (ret < 0)
		dev_err(codec->dev, "Switch registration failed\n");

	cod3022x->input = devm_input_allocate_device(codec->dev);
	if (!cod3022x->input) {
		dev_err(codec->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	/* Not handling Headset events for now.Headset event handling
	 * registered as Input device, causing some conflict with Keyboard Input
	 * device.So, temporarily not handling Headset event, it will be enabled
	 * after proper fix.
	 */
	cod3022x->input->name = "Codec3022 Headset Events";
	cod3022x->input->phys = dev_name(codec->dev);
	cod3022x->input->id.bustype = BUS_I2C;

	cod3022x->input->evbit[0] = BIT_MASK(EV_KEY);
	if (cod3022x->use_btn_adc_mode) {
		for (i = 0; i < num_buttons_zones; i++)
			set_bit(cod3022x->jack_buttons_zones[i].code,
						cod3022x->input->keybit);
	} else {
		set_bit(KEY_MEDIA, cod3022x->input->keybit);
		set_bit(KEY_VOLUMEUP, cod3022x->input->keybit);
		set_bit(KEY_VOLUMEDOWN, cod3022x->input->keybit);
	}

	cod3022x->input->dev.parent = codec->dev;
	input_set_drvdata(cod3022x->input, codec);

	ret = input_register_device(cod3022x->input);
	if (ret != 0) {
		cod3022x->input = NULL;
		dev_err(codec->dev, "Failed to register 3022 input device\n");
	}

	pm_runtime_get_sync(codec->dev);

	/** TODO Enablig all interrupts for now *
	 * but later only necessary interrupts can be enabled *
	 * according to requirement **/
	snd_soc_update_bits(codec, COD3022X_05_IRQ1M, IRQ1M_MASK_ALL, 0x02);
	snd_soc_update_bits(codec, COD3022X_06_IRQ2M, IRQ2M_MASK_ALL, 0x02);

	/* Debounce time initial value set */
	snd_soc_write(codec, COD3022X_84_JACK_DET2, 0x02);
	snd_soc_write(codec, COD3022X_87_LDO_DIG, 0x03);

	snd_soc_update_bits(codec, COD3022X_86_DET_TIME,
			(CTMD_BTN_DBNC_MASK | CTMF_BTN_ON_MASK |
			CTMF_DETB_PERIOD_MASK),
			((CTMD_BTN_DBNC_5 << CTMD_BTN_DBNC_SHIFT) |
			(CTMF_BTN_ON_14_CLK << CTMF_BTN_ON_SHIFT) |
			(detb_period << CTMF_DETB_PERIOD_SHIFT)));

	/** When reboot , the PDB_JD is remained. *
	* So jack detection interrupt does not occur.*
	* reset PDB_JD when boot up time.**/
	snd_soc_update_bits(codec, COD3022X_81_DET_ON,
				EN_PDB_JD_MASK, 0);
	msleep(1);
	if (cod3022x->use_external_jd)
		snd_soc_update_bits(codec, COD3022X_81_DET_ON,
				EN_PDB_JD_CLK_MASK | EN_PDB_JD_MASK,
				EN_PDB_JD_CLK_MASK);
	else {
		snd_soc_update_bits(codec, COD3022X_81_DET_ON,
				EN_PDB_JD_CLK_MASK | EN_PDB_JD_MASK,
				EN_PDB_JD_CLK_MASK | EN_PDB_JD_MASK);

		/* Set Jack debounce time Min when remove 3pole and 4pole jack. */
		snd_soc_write(codec, COD3022X_84_JACK_DET2, 0x3f);
		snd_soc_update_bits(codec, COD3022X_87_LDO_DIG,
				CTMD_JD_DBNC_4_MASK, CTMD_JD_DBNC_4_MASK);
	}

	dev_dbg(codec->dev, "codec DET time reg value=0x%02x @ %s\n",
			snd_soc_read(codec, COD3022X_86_DET_TIME), __func__);

	pm_runtime_put_sync(codec->dev);

	return 0;
}
EXPORT_SYMBOL_GPL(cod3022x_jack_mic_register);

static const struct snd_soc_dai_ops cod3022x_dai_ops = {
	.set_fmt = cod3022x_dai_set_fmt,
	.startup = cod3022x_dai_startup,
	.shutdown = cod3022x_dai_shutdown,
	.hw_params = cod3022x_dai_hw_params,
};

#define COD3022X_RATES		SNDRV_PCM_RATE_8000_192000

#define COD3022X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S20_3LE |	\
				SNDRV_PCM_FMTBIT_S24_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver cod3022x_dai[] = {
	{
		.name = "cod3022x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.ops = &cod3022x_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "cod3022x-aif2",
		.id = 2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.ops = &cod3022x_dai_ops,
		.symmetric_rates = 1,
	},
};

static int cod3022x_regulators_enable(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = regulator_enable(cod3022x->vdd1);
	if (ret)
		return ret;

	ret = regulator_enable(cod3022x->vdd2);
	if (ret)
		return ret;

	return 0;
}

static void cod3022x_regulators_disable(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	regulator_disable(cod3022x->vdd1);
	regulator_disable(cod3022x->vdd2);
}

static void cod3022x_save_otp_registers(struct snd_soc_codec *codec)
{
	int i;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called\n", __func__);
	for(i = 0; i < COD3022X_OTP_MAX_REG; i++) {
		cod3022x->otp_reg[i] = (unsigned char) snd_soc_read(codec,
				(COD3022X_F0_CTRL_IREF1 + i));
	}
}

static void cod3022x_restore_otp_registers(struct snd_soc_codec *codec)
{
	int i;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called\n", __func__);
	for(i = 0; i < COD3022X_OTP_MAX_REG; i++) {
		snd_soc_write(codec, (COD3022X_D0_CTRL_IREF1 + i),
					cod3022x->otp_reg[i]);
	}
}

static void cod3022x_reset_io_selector_bits(struct snd_soc_codec *codec)
{
	/* Reset input selector bits */
	snd_soc_update_bits(codec, COD3022X_75_CHOP_AD,
			EN_LN_CHOP_MASK | EN_MCB1_CHOP_MASK |
			EN_MCB2_CHOP_MASK, 0x0);

	/* Reset output selector bits */
	snd_soc_update_bits(codec, COD3022X_76_CHOP_DA,
			EN_HP_CHOP_MASK | EN_EP_CHOP_MASK |
			EN_SPK_PGA_CHOP_MASK, 0x0);

	/* Reset the mixer switches for AD and DA */
	snd_soc_write(codec, COD3022X_23_MIX_AD, 0x0);
	snd_soc_write(codec, COD3022X_36_MIX_DA1, 0x0);
	snd_soc_write(codec, COD3022X_37_MIX_DA2, 0x0);
}

/*
 * Configure the mic1 and mic2 bias voltages with default value or the value
 * received from the device tree.
 */
static void cod3022x_configure_mic_bias(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	/* Configure Mic1 Bias Voltage */
	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
			CTRV_MCB1_MASK,
			(cod3022x->mic_bias1_voltage << CTRV_MCB1_SHIFT));

	/* Configure Mic2 Bias Voltage */
	snd_soc_update_bits(codec, COD3022X_82_MIC_BIAS,
			CTRV_MCB2_MASK,
			(cod3022x->mic_bias2_voltage << CTRV_MCB2_SHIFT));
}

/**
 * cod3022x_post_fw_update_failure: To be called if f/w update fails
 *
 * In case the firmware is not present or corrupt, we should still be able to
 * run the codec with decent parameters. This values are updated as per the
 * latest stable firmware.
 *
 * The values provided in this function are hard-coded register values, and we
 * need not update these values as per bit-fields.
 */
static void cod3022x_post_fw_update_failure(void *context)
{
	struct snd_soc_codec *codec = (struct snd_soc_codec *)context;

	dev_dbg(codec->dev, "%s called, setting defaults\n", __func__);

	pm_runtime_get_sync(codec->dev);

	/* Default value, enabling HPF and setting freq at 100Hz */
	snd_soc_write(codec, COD3022X_42_ADC1, 0x0c);

	snd_soc_update_bits(codec, COD3022X_71_CLK1_DA,
			SEL_CHCLK_DA_MASK | EN_HALF_CHOP_HP_MASK |
			EN_HALF_CHOP_DA_MASK,
			(DAC_CHOP_CLK_1_BY_32 << SEL_CHCLK_DA_SHIFT) |
			(DAC_HP_PHASE_SEL_3_BY_4 << EN_HALF_CHOP_HP_SHIFT) |
			(DAC_PHASE_SEL_1_BY_4 << EN_HALF_CHOP_DA_SHIFT));

	snd_soc_update_bits(codec, COD3022X_D0_CTRL_IREF1,
			CTMI_VCM_MASK | CTMI_MIX_MASK,
			(CTMI_VCM_4U << CTMI_VCM_SHIFT) | CTMI_MIX_2U);

	snd_soc_update_bits(codec, COD3022X_D1_CTRL_IREF2,
			CTMI_INT1_MASK, CTMI_INT1_4U);

	snd_soc_update_bits(codec, COD3022X_D2_CTRL_IREF3,
			CTMI_MIC2_MASK | CTMI_MIC1_MASK,
			(CTMI_MIC2_2U << CTMI_MIC2_SHIFT) | CTMI_MIC1_2U);

	snd_soc_update_bits(codec, COD3022X_D3_CTRL_IREF4,
			CTMI_BUFF_MASK | CTMI_LN_MASK,
			(CTMI_BUFF_2U << CTMI_BUFF_SHIFT) | CTMI_LN_2U);

	snd_soc_write(codec, COD3022X_43_ADC_L_VOL, 0x18);
	snd_soc_write(codec, COD3022X_44_ADC_R_VOL, 0x00);
	/* Boost 20 dB, Gain 0 dB for MIC1 */
	snd_soc_write(codec, COD3022X_20_VOL_AD1, 0x54);
	/* Gain 6dB for Line-in */
	snd_soc_write(codec, COD3022X_22_VOL_AD3, 0x66);
	snd_soc_write(codec, COD3022X_5A_DNC7, 0x18);


	/* Update for 3-pole jack detection */
	snd_soc_write(codec, COD3022X_85_MIC_DET, 0x03);

	snd_soc_write(codec, COD3022X_70_CLK1_AD, 0x07);

	/* Reset input/output selector bits */
	cod3022x_reset_io_selector_bits(codec);

	/* Configure mic bias voltage */
	cod3022x_configure_mic_bias(codec);

	pm_runtime_put_sync(codec->dev);
}

/**
 * cod3022x_post_fw_update_success: To be called after f/w update
 *
 * The firmware may be enabling some of the path and power registers which are
 * used during path enablement. We need to keep the values of these registers
 * consistent so that the functionality of the codec driver doesn't change
 * because of the firmware.
 */
static void cod3022x_post_fw_update_success(void *context)
{
	struct snd_soc_codec *codec = (struct snd_soc_codec *)context;

	dev_dbg(codec->dev, "%s called\n", __func__);

	pm_runtime_get_sync(codec->dev);

	/* Update for 3-pole jack detection */
	snd_soc_write(codec, COD3022X_85_MIC_DET, 0x03);

	/* Reset input/output selector bits */
	cod3022x_reset_io_selector_bits(codec);

	/* Reset the mixer switches for AD and DA */
	snd_soc_write(codec, COD3022X_23_MIX_AD, 0x0);
	snd_soc_write(codec, COD3022X_36_MIX_DA1, 0x0);
	snd_soc_write(codec, COD3022X_37_MIX_DA2, 0x0);

	/* Reset the auto power bits for AD */
	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_LN_MASK |
			APW_MIC1_MASK | APW_MIC2_MASK,
			0x0);

	/* Reset the auto power bits for DA */
	snd_soc_update_bits(codec, COD3022X_17_PWAUTO_DA,
			PW_AUTO_DA_MASK | APW_SPK_MASK |
			APW_HP_MASK | APW_EP_MASK,
			0x0);

	/* Configure mic bias voltage */
	cod3022x_configure_mic_bias(codec);

	snd_soc_write(codec, COD3022X_70_CLK1_AD, 0x07);

	/*
	 * Need to restore back the device specific OTP values as the firmware
	 * binary might have corrupted the OTP values
	 */
	cod3022x_restore_otp_registers(codec);

	pm_runtime_put_sync(codec->dev);
}

static void cod3022x_regmap_sync(struct device *dev)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);
	unsigned char reg[COD3022X_MAX_REGISTER];
	int i;

	/* Read from Cache */
	for (i = 0; i < COD3022X_D0_CTRL_IREF1; i++)
		if (cod3022x_readable_register(dev, i) &&
				(!cod3022x_volatile_register(dev,i)))
			reg[i] = (unsigned char)
				snd_soc_read(cod3022x->codec, i);

	regcache_cache_bypass(cod3022x->regmap, true);

	snd_soc_write(cod3022x->codec,COD3022X_81_DET_ON,
					reg[COD3022X_81_DET_ON]);
	snd_soc_write(cod3022x->codec, COD3022X_40_DIGITAL_POWER,
					reg[COD3022X_40_DIGITAL_POWER]);

	/* Update HW */
	for (i = 0; i < COD3022X_D0_CTRL_IREF1; i++)
		if (cod3022x_writeable_register(dev, i) &&
				(!cod3022x_volatile_register(dev,i)))
			snd_soc_write(cod3022x->codec, i, reg[i]);

	regcache_cache_bypass(cod3022x->regmap, false);
}

static void cod3022x_reg_restore(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, COD3022X_81_DET_ON,
			EN_PDB_JD_CLK_MASK, EN_PDB_JD_CLK_MASK);

	/* Give 15ms delay before accessing the otp values */
	usleep_range(15000, 15000 + 1000);

	/*
	 * The OTP values are the boot-time values. For registers D0-DE, we need
	 * to save these register values during boot time. After system reset,
	 * these values are lost and we need to restore them using saved values.
	 */
	if (!cod3022x->is_probe_done) {
		cod3022x_reset_io_selector_bits(codec);
		cod3022x_save_otp_registers(codec);
	} else {
		cod3022x_regmap_sync(codec->dev);
		cod3022x_restore_otp_registers(cod3022x->codec);
	}
}

static void cod3022x_i2c_parse_dt(struct cod3022x_priv *cod3022x)
{
	/* todo .. Need to add DT parsing for 3022 */
	struct device *dev = cod3022x->dev;
	struct device_node *np = dev->of_node;
	unsigned int bias_v_conf;
	struct of_phandle_args args;
	int i = 0;
	int mic_range, mic_delay, btn_rel_val;
	int num_buttons_zones = ARRAY_SIZE(cod3022x->jack_buttons_zones);
	int ret;

	cod3022x->int_gpio = of_get_gpio(np, 0);

	if (!gpio_is_valid(cod3022x->int_gpio))
		dev_err(dev, "(*)Error in getting Codec-3022 Interrupt gpio\n");

	/* Mic bias set default 3.0v */
	cod3022x->mic_bias1_voltage = MIC_BIAS1_VO_3_0V;
	cod3022x->mic_bias2_voltage = MIC_BIAS2_VO_3_0V;

	ret = of_property_read_u32(dev->of_node,
				"mic-bias1-voltage", &bias_v_conf);
	if ((!ret ) && ((bias_v_conf >= MIC_BIAS1_VO_2_1V) &&
			(bias_v_conf <= MIC_BIAS1_VO_3_0V)))
		cod3022x->mic_bias1_voltage = bias_v_conf;
	else
		dev_dbg(dev, "Property 'mic-bias1-voltage' %s",
				ret ? "not found" :
				"invalid value (use:0-3)");

	ret = of_property_read_u32(dev->of_node,
				"mic-bias2-voltage", &bias_v_conf);
	if ((!ret ) && ((bias_v_conf >= MIC_BIAS2_VO_2_1V) &&
			(bias_v_conf <= MIC_BIAS2_VO_3_0V)))
		cod3022x->mic_bias2_voltage = bias_v_conf;
	else
		dev_dbg(dev, "Property 'mic-bias2-voltage' %s",
				ret ? "not found" :
				"invalid value (use:0-3)");

	dev_dbg(dev,"Bias voltage conf value bias1 = %d, bias2= %d\n",
			cod3022x->mic_bias1_voltage,
			cod3022x->mic_bias2_voltage);

	if (of_find_property(dev->of_node, "use-btn-adc-mode", NULL) != NULL)
		cod3022x->use_btn_adc_mode = true;

	dev_dbg(dev, "Using %s for button detection\n",
			cod3022x->use_btn_adc_mode ? "GPADC" : "internal h/w");

	ret = of_property_read_u32(dev->of_node, "mic-adc-range", &mic_range);
	if (!ret)
		cod3022x->mic_adc_range = mic_range;
	else
		cod3022x->mic_adc_range = 1120;

	ret = of_property_read_u32(dev->of_node, "mic-det-delay", &mic_delay);
	if (!ret)
		cod3022x->mic_det_delay = mic_delay;
	else
		cod3022x->mic_det_delay = 50;

	ret = of_property_read_u32(dev->of_node, "btn-release-value", &btn_rel_val);
	if (!ret)
		cod3022x->btn_release_value = btn_rel_val;
	else
		cod3022x->btn_release_value = 1100;

	if (cod3022x->use_btn_adc_mode) {
		/* Parsing but-zones, a maximum of 4 buttons are supported */
		for (i = 0; i < num_buttons_zones; i++) {
			if (of_parse_phandle_with_args(dev->of_node,
				"but-zones-list", "#list-but-cells", i, &args))
				break;

			cod3022x->jack_buttons_zones[i].code = args.args[0];
			cod3022x->jack_buttons_zones[i].adc_low = args.args[1];
			cod3022x->jack_buttons_zones[i].adc_high = args.args[2];
		}
		/* initialize button status */
		cod3022x->jack_det.privious_button_state = BUTTON_RELEASE;

		for (i = 0; i < num_buttons_zones; i++)
			dev_dbg(dev, "[DEBUG]: buttons: code(%d), low(%d), high(%d)\n",
				cod3022x->jack_buttons_zones[i].code,
				cod3022x->jack_buttons_zones[i].adc_low,
				cod3022x->jack_buttons_zones[i].adc_high);
	}

	if (of_find_property(dev->of_node, "update-firmware", NULL))
		cod3022x->update_fw = true;
	else
		cod3022x->update_fw = false;

	if (of_find_property(dev->of_node, "mic-uses-auto-power-mode", NULL))
		cod3022x->use_auto_mic_power_on = true;
	else
		cod3022x->use_auto_mic_power_on = false;

}

static int cod3022x_codec_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "(*) %s\n", __func__);

	cod3022x->codec = codec;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret)
		return ret;

	cod3022x->vdd1 = devm_regulator_get(codec->dev, "vdd1");
	if (IS_ERR(cod3022x->vdd1)) {
		dev_warn(codec->dev, "failed to get regulator vdd1\n");
		return PTR_ERR(cod3022x->vdd1);
	}

	cod3022x->vdd2 = devm_regulator_get(codec->dev, "vdd2");
	if (IS_ERR(cod3022x->vdd2)) {
		dev_warn(codec->dev, "failed to get regulator vdd2\n");
		return PTR_ERR(cod3022x->vdd2);
	}

	pm_runtime_get_sync(codec->dev);
	cod3022x->is_probe_done = true;

	/* Initialize work queue for button handling */
	INIT_DELAYED_WORK(&cod3022x->buttons_work, cod3022x_buttons_work);

	cod3022x->buttons_wq = create_singlethread_workqueue("buttons_wq");
	if (cod3022x->buttons_wq == NULL) {
		dev_err(codec->dev, "Failed to create buttons_wq\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&cod3022x->jack_det_work , cod3022x_jack_det_work);

	cod3022x->jack_det_wq = create_singlethread_workqueue("jack_det_wq");
	if (cod3022x->jack_det_wq == NULL) {
		dev_err(codec->dev, "Failed to create jack_det_wq\n");
		return -ENOMEM;
	}

	cod3022x_adc_start(cod3022x);

	/* adc mixer setting initialised */
	cod3022x->adc_mix_reg_val = 0;

	cod3022x->aifrate = COD3022X_SAMPLE_RATE_48KHZ;
	cod3022x_i2c_parse_dt(cod3022x);

	if (cod3022x->update_fw)
		exynos_regmap_update_fw(COD3022X_FIRMWARE_NAME,
			codec->dev, cod3022x->regmap, cod3022x->i2c_addr,
			cod3022x_post_fw_update_success, codec,
			cod3022x_post_fw_update_failure, codec);
	else
		cod3022x_post_fw_update_failure(codec);

	if (gpio_is_valid(cod3022x->int_gpio)) {
		ret = gpio_request(cod3022x->int_gpio, "cod3022x_irq");
		if (ret < 0) {
			dev_err(codec->dev, "%s : Request for %d GPIO failed\n",
					__func__, (int)cod3022x->int_gpio);
		}

		ret = gpio_direction_input(cod3022x->int_gpio);
		if (ret < 0) {
			dev_err(codec->dev,
			"Setting 3022 interrupt GPIO direction to input :failed\n");
		}
		mutex_init(&cod3022x->jackdet_lock);
		mutex_init(&cod3022x->key_lock);

		ret = request_threaded_irq(
				gpio_to_irq(cod3022x->int_gpio),
				NULL, cod3022x_threaded_isr,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"cod3022_theaded_isr", cod3022x);
		if (ret < 0) {
			dev_err(codec->dev,
			"Error %d in requesting 3022 interrupt line:%d\n",
					ret, cod3022x->int_gpio);
		}

		ret = irq_set_irq_wake(gpio_to_irq(cod3022x->int_gpio), 1);
		if (ret < 0)
			dev_err(codec->dev, "cannot set 3022 irq_set_irq_wake\n");

	}

	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPKOUTLN");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "HPOUTLN");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "EPOUTN");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "IN1L");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "IN2L");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "IN3L");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_sync(&codec->dapm);

	pm_runtime_put_sync(codec->dev);

	return 0;
}

static int cod3022x_codec_remove(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "(*) %s called\n", __func__);

	cancel_delayed_work_sync(&cod3022x->key_work);
	if (gpio_is_valid(cod3022x->int_gpio)) {
		free_irq(gpio_to_irq(cod3022x->int_gpio), cod3022x);
		gpio_free(cod3022x->int_gpio);
	}

	cod3022x_regulators_disable(codec);

	destroy_workqueue(cod3022x->buttons_wq);
	destroy_workqueue(cod3022x->jack_det_wq);

	cod3022x_adc_stop(cod3022x);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_cod3022x = {
	.probe = cod3022x_codec_probe,
	.remove = cod3022x_codec_remove,
	.controls = cod3022x_snd_controls,
	.num_controls = ARRAY_SIZE(cod3022x_snd_controls),
	.dapm_widgets = cod3022x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cod3022x_dapm_widgets),
	.dapm_routes = cod3022x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(cod3022x_dapm_routes),
	.ignore_pmdown_time = true,
	.idle_bias_off = true,
};

/* To support PBA function test */
#ifndef CONFIG_SAMSUNG_JACK
#include "../samsung/jack_carmen.c"
#endif

static int cod3022x_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct cod3022x_priv *cod3022x;
	struct pinctrl *pinctrl;
	int ret;

	cod3022x = kzalloc(sizeof(struct cod3022x_priv), GFP_KERNEL);
	if (cod3022x == NULL)
		return -ENOMEM;
	cod3022x->dev = &i2c->dev;
	cod3022x->i2c_addr = i2c->addr;
	cod3022x->use_external_jd = false;
	cod3022x->is_probe_done = false;
	cod3022x->use_btn_adc_mode = false;
	cod3022x->int_gpio = -EINVAL;
	g_cod3022x = cod3022x;

	cod3022x->regmap = devm_regmap_init_i2c(i2c, &cod3022x_regmap);
	if (IS_ERR(cod3022x->regmap)) {
		dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
		return PTR_ERR(cod3022x->regmap);
	}

	pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(pinctrl)) {
		dev_warn(&i2c->dev, "did not get pins for codec: %li\n",
							PTR_ERR(pinctrl));
	} else {
		cod3022x->pinctrl = pinctrl;
	}

	i2c_set_clientdata(i2c, cod3022x);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_cod3022x,
			cod3022x_dai, ARRAY_SIZE(cod3022x_dai));
	if (ret < 0)
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);

/* To support PBA function test */
#ifndef CONFIG_SAMSUNG_JACK
	create_jack_devices(cod3022x);
#endif

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(cod3022x->dev);
#endif
	return ret;
}

static int cod3022x_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	return 0;
}

static void cod3022x_cfg_gpio(struct device *dev, const char *name)
{
	struct pinctrl_state *pin_state;
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	pin_state = pinctrl_lookup_state(cod3022x->pinctrl, name);
	if (IS_ERR(pin_state))
		goto err;

	if (pinctrl_select_state(cod3022x->pinctrl, pin_state) < 0)
		goto err;

	return;
err:
	dev_err(dev, "Unable to configure codec gpio as %s\n", name);
	return;
}

static int cod3022x_enable(struct device *dev)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);

	cod3022x_cfg_gpio(dev, "default");
	cod3022x_regulators_enable(cod3022x->codec);

	/*
	 * Below sequence should be maintained, so that even the jd interupt
	 * changes the cache mode between below two line should not cause
	 * issue
	 */
	cod3022x->is_suspend = false;
	regcache_cache_only(cod3022x->regmap, false);
	cod3022x_reg_restore(cod3022x->codec);

	return 0;
}

static int cod3022x_disble(struct device *dev)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);

	/*
	 * Below sequence should be maintained, so that even the jd interupt
	 * changes the cache mode between below two line should not cause
	 * issue
	 */
	cod3022x->is_suspend = true;
	regcache_cache_only(cod3022x->regmap, true);
	cod3022x_regulators_disable(cod3022x->codec);
	cod3022x_cfg_gpio(dev, "idle");

	return 0;
}

static int cod3022x_sys_suspend(struct device *dev)
{
#ifndef CONFIG_PM_RUNTIME
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	if (is_cp_aud_enabled()) {
		dev_dbg(dev, "(*)Don't suspend Codec-3022, cp functioning\n");
		return 0;
	}
	dev_dbg(dev, "(*) %s\n", __func__);
	cod3022x_disble(dev);
#endif

	return 0;
}

static int cod3022x_sys_resume(struct device *dev)
{
#ifndef CONFIG_PM_RUNTIME
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	if (!cod3022x->is_suspend) {
		dev_dbg(dev, "(*)Codec-3022 not resuming, cp functioning\n");
		return 0;
	}
	dev_dbg(dev, "(*) %s\n", __func__);
	cod3022x_enable(dev);
#endif

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int cod3022x_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "(*) %s\n", __func__);

	s2801x_get_sync();
	cod3022x_enable(dev);

	return 0;
}

static int cod3022x_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "(*) %s\n", __func__);

	cod3022x_disble(dev);
	s2801x_put_sync();

	return 0;
}
#endif

static const struct dev_pm_ops cod3022x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			cod3022x_sys_suspend,
			cod3022x_sys_resume
	)
	SET_RUNTIME_PM_OPS(
			cod3022x_runtime_suspend,
			cod3022x_runtime_resume,
			NULL
	)
};

static const struct i2c_device_id cod3022x_i2c_id[] = {
	{ "cod3022x", 3022 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cod3022x_i2c_id);

const struct of_device_id cod3022x_of_match[] = {
	{ .compatible = "codec,cod3022x",},
	{},
};

static struct i2c_driver cod3022x_i2c_driver = {
	.driver = {
		.name = "cod3022x",
		.owner = THIS_MODULE,
		.pm = &cod3022x_pm,
		.of_match_table = of_match_ptr(cod3022x_of_match),
	},
	.probe = cod3022x_i2c_probe,
	.remove = cod3022x_i2c_remove,
	.id_table = cod3022x_i2c_id,
};

module_i2c_driver(cod3022x_i2c_driver);

MODULE_DESCRIPTION("ASoC COD3022X driver");
MODULE_AUTHOR("Sayanta Pattanayak <sayanta.p@samsung.com>");
MODULE_AUTHOR("Tushar Behera <tushar.b@samsung.com>");
MODULE_AUTHOR("R Chandrasekar <rcsekar@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:COD3022X-codec");
MODULE_FIRMWARE(COD3022X_FIRMWARE_NAME);
