// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI I2S Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/bitops.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt6989-afe-clk.h"
#include "mt6989-afe-common.h"
#include "mt6989-afe-gpio.h"
#include "mt6989-interconnection.h"
#define FM_SRC_CAIL
enum {
	ETDM_CLK_SOURCE_H26M = 0,
	ETDM_CLK_SOURCE_APLL = 1,
	ETDM_CLK_SOURCE_SPDIF = 2,
	ETDM_CLK_SOURCE_HDMI = 3,
	ETDM_CLK_SOURCE_EARC = 4,
	ETDM_CLK_SOURCE_LINEIN = 5,
};
enum {
	ETDM_RELATCH_SEL_H26M = 0,
	ETDM_RELATCH_SEL_APLL = 1,
};
enum {
	ETDM_RATE_8K = 0,
	ETDM_RATE_12K = 1,
	ETDM_RATE_16K = 2,
	ETDM_RATE_24K = 3,
	ETDM_RATE_32K = 4,
	ETDM_RATE_48K = 5,
	ETDM_RATE_64K = 6, //not support
	ETDM_RATE_96K = 7,
	ETDM_RATE_128K = 8, //not support
	ETDM_RATE_192K = 9,
	ETDM_RATE_256K = 10, //not support
	ETDM_RATE_384K = 11,
	ETDM_RATE_11025 = 16,
	ETDM_RATE_22050 = 17,
	ETDM_RATE_44100 = 18,
	ETDM_RATE_88200 = 19,
	ETDM_RATE_176400 = 20,
	ETDM_RATE_352800 = 21,
};

enum {
	ETDM_CONN_8K = 0,
	ETDM_CONN_11K = 1,
	ETDM_CONN_12K = 2,
	ETDM_CONN_16K = 4,
	ETDM_CONN_22K = 5,
	ETDM_CONN_24K = 6,
	ETDM_CONN_32K = 8,
	ETDM_CONN_44K = 9,
	ETDM_CONN_48K = 10,
	ETDM_CONN_88K = 13,
	ETDM_CONN_96K = 14,
	ETDM_CONN_176K = 17,
	ETDM_CONN_192K = 18,
	ETDM_CONN_352K = 21,
	ETDM_CONN_384K = 22,
};
enum {
	ETDM_WLEN_8_BIT = 0x7,
	ETDM_WLEN_16_BIT = 0xf,
	ETDM_WLEN_32_BIT = 0x1f,
};
enum {
	ETDM_SLAVE_SEL_ETDMIN0_MASTER = 0,
	ETDM_SLAVE_SEL_ETDMIN0_SLAVE = 1,
	ETDM_SLAVE_SEL_ETDMIN1_MASTER = 2,
	ETDM_SLAVE_SEL_ETDMIN1_SLAVE = 3,
	ETDM_SLAVE_SEL_ETDMIN2_MASTER = 4,
	ETDM_SLAVE_SEL_ETDMIN2_SLAVE = 5,
	ETDM_SLAVE_SEL_ETDMIN3_MASTER = 6,
	ETDM_SLAVE_SEL_ETDMIN3_SLAVE = 7,
	ETDM_SLAVE_SEL_ETDMOUT0_MASTER = 8,
	ETDM_SLAVE_SEL_ETDMOUT0_SLAVE = 9,
	ETDM_SLAVE_SEL_ETDMOUT1_MASTER = 10,
	ETDM_SLAVE_SEL_ETDMOUT1_SLAVE = 11,
	ETDM_SLAVE_SEL_ETDMOUT2_MASTER = 12,
	ETDM_SLAVE_SEL_ETDMOUT2_SLAVE = 13,
	ETDM_SLAVE_SEL_ETDMOUT3_MASTER = 14,
	ETDM_SLAVE_SEL_ETDMOUT3_SLAVE = 15,
};

enum {
	ETDM_SLAVE_SEL_ETDMIN4_MASTER = 0,
	ETDM_SLAVE_SEL_ETDMIN4_SLAVE = 1,
	ETDM_SLAVE_SEL_ETDMIN5_MASTER = 2,
	ETDM_SLAVE_SEL_ETDMIN5_SLAVE = 3,
	ETDM_SLAVE_SEL_ETDMIN6_MASTER = 4,
	ETDM_SLAVE_SEL_ETDMIN6_SLAVE = 5,
	ETDM_SLAVE_SEL_ETDMIN7_MASTER = 6,
	ETDM_SLAVE_SEL_ETDMIN7_SLAVE = 7,
	ETDM_SLAVE_SEL_ETDMOUT4_MASTER = 8,
	ETDM_SLAVE_SEL_ETDMOUT4_SLAVE = 9,
	ETDM_SLAVE_SEL_ETDMOUT5_MASTER = 10,
	ETDM_SLAVE_SEL_ETDMOUT5_SLAVE = 11,
	ETDM_SLAVE_SEL_ETDMOUT6_MASTER = 12,
	ETDM_SLAVE_SEL_ETDMOUT6_SLAVE = 13,
	ETDM_SLAVE_SEL_ETDMOUT7_MASTER = 14,
	ETDM_SLAVE_SEL_ETDMOUT7_SLAVE = 15,
};

static unsigned int get_etdm_wlen(snd_pcm_format_t format)
{
	unsigned int wlen = 0;

	/* The reg_word_length should be >= reg_bit_length */
	wlen = snd_pcm_format_physical_width(format);

	if (wlen < 16)
		return ETDM_WLEN_16_BIT;
	else
		return ETDM_WLEN_32_BIT;
}

static unsigned int get_etdm_lrck_width(snd_pcm_format_t format)
{
	/* The valid data bit number should be large than 7 due to hardware limitation. */
	return snd_pcm_format_physical_width(format) - 1;

}

static unsigned int get_etdm_rate(unsigned int rate)
{
	switch (rate) {
	case 8000:
		return ETDM_RATE_8K;
	case 12000:
		return ETDM_RATE_12K;
	case 16000:
		return ETDM_RATE_16K;
	case 24000:
		return ETDM_RATE_24K;
	case 32000:
		return ETDM_RATE_32K;
	case 48000:
		return ETDM_RATE_48K;
	case 64000:
		return ETDM_RATE_64K;
	case 96000:
		return ETDM_RATE_96K;
	case 128000:
		return ETDM_RATE_128K;
	case 192000:
		return ETDM_RATE_192K;
	case 256000:
		return ETDM_RATE_256K;
	case 384000:
		return ETDM_RATE_384K;
	case 11025:
		return ETDM_RATE_11025;
	case 22050:
		return ETDM_RATE_22050;
	case 44100:
		return ETDM_RATE_44100;
	case 88200:
		return ETDM_RATE_88200;
	case 176400:
		return ETDM_RATE_176400;
	case 352800:
		return ETDM_RATE_352800;
	default:
		return 0;
	}
}

static unsigned int get_etdm_inconn_rate(unsigned int rate)
{
	switch (rate) {
	case 8000:
		return ETDM_CONN_8K;
	case 12000:
		return ETDM_CONN_12K;
	case 16000:
		return ETDM_CONN_16K;
	case 24000:
		return ETDM_CONN_24K;
	case 32000:
		return ETDM_CONN_32K;
	case 48000:
		return ETDM_CONN_48K;
	case 96000:
		return ETDM_CONN_96K;
	case 192000:
		return ETDM_CONN_192K;
	case 384000:
		return ETDM_CONN_384K;
	case 11025:
		return ETDM_CONN_11K;
	case 22050:
		return ETDM_CONN_22K;
	case 44100:
		return ETDM_CONN_44K;
	case 88200:
		return ETDM_CONN_88K;
	case 176400:
		return ETDM_CONN_176K;
	case 352800:
		return ETDM_CONN_352K;
	default:
		return 0;
	}

}

struct mtk_afe_i2s_priv {
	int id;
	int rate; /* for determine which apll to use */
	int low_jitter_en;

	const char *share_property_name;
	int share_i2s_id;

	int mclk_id;
	int mclk_rate;
	int mclk_apll;

	int ch_num;
	int sync;
	int ip_mode;
	int lpbk_mode;
};

/* lpbk */
static const int etdm_lpbk_idx_0[] = {
	0x0, 0x8,
};
static const int etdm_lpbk_idx_1[] = {
	0x2, 0xa,
};
static const int etdm_lpbk_idx_2[] = {
	0x4, 0xc,
};

static int etdm_lpbk_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int value = 0;
	unsigned int value_ipmode = 0;
	unsigned int reg = 0;
	unsigned int mask = 0;
	unsigned int shift = 0;

	if (!strcmp(kcontrol->id.name, "I2SIN0_LPBK")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN0_SDATA0_SEL_MASK_SFT;
		shift = ETDM_IN0_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "I2SIN1_LPBK")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN1_SDATA0_SEL_MASK_SFT;
		shift = ETDM_IN1_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "I2SIN2_LPBK")) {
		reg = ETDM_0_3_COWORK_CON3;
		mask = ETDM_IN2_SDATA0_SEL_MASK_SFT;
		shift = ETDM_IN2_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "I2SIN4_LPBK")) {
		reg = ETDM_4_7_COWORK_CON1;

		// Get I2SIN4 multi-ip mode
		regmap_read(afe->regmap, ETDM_IN4_CON2, &value_ipmode);
		value_ipmode &= REG_MULTI_IP_MODE_MASK_SFT;
		value_ipmode >>= REG_MULTI_IP_MODE_SFT;

		if (value_ipmode) {
			mask = ETDM_IN4_SDATA1_15_SEL_MASK_SFT;
			shift = ETDM_IN4_SDATA1_15_SEL_SFT;
		} else {
			mask = ETDM_IN4_SDATA0_SEL_MASK_SFT;
			shift = ETDM_IN4_SDATA0_SEL_SFT;
		}
	} else if (!strcmp(kcontrol->id.name, "I2SIN6_LPBK")) {
		reg = ETDM_4_7_COWORK_CON3;
		mask = ETDM_IN6_SDATA0_SEL_MASK_SFT;
		shift = ETDM_IN6_SDATA0_SEL_SFT;
	}

	if (reg)
		regmap_read(afe->regmap, reg, &value);

	value &= mask;
	value >>= shift;
	ucontrol->value.enumerated.item[0] = value;

	if (value == 0x8 || value == 0xa || value == 0xc)
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;

	return 0;
}

static int etdm_lpbk_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int value_ipmode = 0;
	unsigned int reg = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	if (value >= ARRAY_SIZE(etdm_lpbk_idx_0))
		return -EINVAL;

	if (!strcmp(kcontrol->id.name, "I2SIN0_LPBK")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN0_SDATA0_SEL_MASK_SFT;
		val = etdm_lpbk_idx_0[value] << ETDM_IN0_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "I2SIN1_LPBK")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN1_SDATA0_SEL_MASK_SFT;
		val = etdm_lpbk_idx_1[value] << ETDM_IN1_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "I2SIN2_LPBK")) {
		reg = ETDM_0_3_COWORK_CON3;
		mask = ETDM_IN2_SDATA0_SEL_MASK_SFT;
		val = etdm_lpbk_idx_2[value] << ETDM_IN2_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "I2SIN4_LPBK")) {
		reg = ETDM_4_7_COWORK_CON1;

		// Get I2SIN4 multi-ip mode
		regmap_read(afe->regmap, ETDM_IN4_CON2, &value_ipmode);
		value_ipmode &= REG_MULTI_IP_MODE_MASK_SFT;
		value_ipmode >>= REG_MULTI_IP_MODE_SFT;

		if (!value) {
			mask = ETDM_IN4_SDATA1_15_SEL_MASK_SFT |
				ETDM_IN4_SDATA0_SEL_MASK_SFT;
			val = (etdm_lpbk_idx_0[value] << ETDM_IN4_SDATA1_15_SEL_SFT) |
				(etdm_lpbk_idx_0[value] << ETDM_IN4_SDATA0_SEL_SFT);
		} else if (value_ipmode) {
			mask = ETDM_IN4_SDATA1_15_SEL_MASK_SFT;
			val = etdm_lpbk_idx_0[value] << ETDM_IN4_SDATA1_15_SEL_SFT;
		} else {
			mask = ETDM_IN4_SDATA0_SEL_MASK_SFT;
			val = etdm_lpbk_idx_0[value] << ETDM_IN4_SDATA0_SEL_SFT;
		}
	} else {
		reg = ETDM_4_7_COWORK_CON3;
		mask = ETDM_IN6_SDATA0_SEL_MASK_SFT;
		val = etdm_lpbk_idx_2[value] << ETDM_IN6_SDATA0_SEL_SFT;
	}

	if (reg)
		regmap_update_bits(afe->regmap, reg, mask, val);

	return 0;
}
static const char *const etdm_lpbk_map[] = {
	"Off", "On",
};
static SOC_ENUM_SINGLE_EXT_DECL(etdm_lpbk_map_enum,
				etdm_lpbk_map);
/* lpbk */

/* multi-ip mode */
static const int etdm_ip_mode_idx[] = {
	0x0, 0x1,
};
static int etdm_ip_mode_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2sin4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_IN4];

	ucontrol->value.enumerated.item[0] = i2sin4_priv->ip_mode;

	return 0;
}

static int etdm_ip_mode_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2sin4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_IN4];
	unsigned int value = ucontrol->value.integer.value[0];

	if (value >= ARRAY_SIZE(etdm_ip_mode_idx))
		return -EINVAL;

	/* 0: One IP multi-channel 1: Multi-IP 2-channel */
	i2sin4_priv->ip_mode = etdm_ip_mode_idx[value];

	return 0;
}
static const char *const etdm_ip_mode_map[] = {
	"Off", "On",
};
static SOC_ENUM_SINGLE_EXT_DECL(etdm_ip_mode_map_enum,
				etdm_ip_mode_map);
/* multi-ip mode */

/* ch num */
static const int etdm_ch_num_idx[] = {
	0x2, 0x4, 0x6, 0x8,
};
static int etdm_ch_num_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2sin4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_IN4];
	struct mtk_afe_i2s_priv *i2sout4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_OUT4];
	unsigned int value = 0;

	if (!strcmp(kcontrol->id.name, "I2SIN4_CH_NUM"))
		value = i2sin4_priv->ch_num;
	else if (!strcmp(kcontrol->id.name, "I2SOUT4_CH_NUM"))
		value = i2sout4_priv->ch_num;

	if (value == 0x2)
		ucontrol->value.enumerated.item[0] = 0;
	else if (value == 0x4)
		ucontrol->value.enumerated.item[0] = 1;
	else if (value == 0x6)
		ucontrol->value.enumerated.item[0] = 2;
	else
		ucontrol->value.enumerated.item[0] = 3;

	return 0;
}

static int etdm_ch_num_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2sin4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_IN4];
	struct mtk_afe_i2s_priv *i2sout4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_OUT4];
	unsigned int value = ucontrol->value.integer.value[0];

	if (value >= ARRAY_SIZE(etdm_ch_num_idx))
		return -EINVAL;

	if (!strcmp(kcontrol->id.name, "I2SIN4_CH_NUM"))
		i2sin4_priv->ch_num = etdm_ch_num_idx[value];
	else if (!strcmp(kcontrol->id.name, "I2SOUT4_CH_NUM"))
		i2sout4_priv->ch_num = etdm_ch_num_idx[value];

	return 0;
}
static const char *const etdm_ch_num_map[] = {
	"2CH", "4CH", "6CH", "8CH",
};
static SOC_ENUM_SINGLE_EXT_DECL(etdm_ch_num_map_enum,
				etdm_ch_num_map);
/* ch num */

enum {
	I2S_FMT_EIAJ = 0,
	I2S_FMT_I2S = 1,
};

enum {
	I2S_WLEN_16_BIT = 0,
	I2S_WLEN_32_BIT = 1,
};

enum {
	I2S_HD_NORMAL = 0,
	I2S_HD_LOW_JITTER = 1,
};

enum {
	I2S1_SEL_O28_O29 = 0,
	I2S1_SEL_O03_O04 = 1,
};

enum {
	I2S_IN_PAD_CONNSYS = 0,
	I2S_IN_PAD_IO_MUX = 1,
};

static unsigned int get_i2s_wlen(snd_pcm_format_t format)
{
	return snd_pcm_format_physical_width(format) <= 16 ?
	       I2S_WLEN_16_BIT : I2S_WLEN_32_BIT;
}

#define MTK_AFE_I2SIN0_KCONTROL_NAME "I2SIN0_HD_Mux"
#define MTK_AFE_I2SIN1_KCONTROL_NAME "I2SIN1_HD_Mux"
#define MTK_AFE_I2SIN2_KCONTROL_NAME "I2SIN2_HD_Mux"
#define MTK_AFE_I2SIN4_KCONTROL_NAME "I2SIN4_HD_Mux"
#define MTK_AFE_I2SIN6_KCONTROL_NAME "I2SIN6_HD_Mux"
#define MTK_AFE_I2SOUT0_KCONTROL_NAME "I2SOUT0_HD_Mux"
#define MTK_AFE_I2SOUT1_KCONTROL_NAME "I2SOUT1_HD_Mux"
#define MTK_AFE_I2SOUT2_KCONTROL_NAME "I2SOUT2_HD_Mux"
#define MTK_AFE_I2SOUT4_KCONTROL_NAME "I2SOUT4_HD_Mux"
#define MTK_AFE_I2SOUT6_KCONTROL_NAME "I2SOUT6_HD_Mux"
#define MTK_AFE_FMI2S_MASTER_KCONTROL_NAME "FMI2S_MASTER_HD_Mux"

#define I2SIN0_HD_EN_W_NAME "I2SIN0_HD_EN"
#define I2SIN1_HD_EN_W_NAME "I2SIN1_HD_EN"
#define I2SIN2_HD_EN_W_NAME "I2SIN2_HD_EN"
#define I2SIN4_HD_EN_W_NAME "I2SIN4_HD_EN"
#define I2SIN6_HD_EN_W_NAME "I2SIN6_HD_EN"
#define I2SOUT0_HD_EN_W_NAME "I2SOUT0_HD_EN"
#define I2SOUT1_HD_EN_W_NAME "I2SOUT1_HD_EN"
#define I2SOUT2_HD_EN_W_NAME "I2SOUT2_HD_EN"
#define I2SOUT4_HD_EN_W_NAME "I2SOUT4_HD_EN"
#define I2SOUT6_HD_EN_W_NAME "I2SOUT6_HD_EN"
#define FMI2S_MASTER_HD_EN_W_NAME "FMI2S_MASTER_HD_EN"

#define I2SIN0_MCLK_EN_W_NAME "I2SIN0_MCLK_EN"
#define I2SIN1_MCLK_EN_W_NAME "I2SIN1_MCLK_EN"
#define I2SIN2_MCLK_EN_W_NAME "I2SIN2_MCLK_EN"
#define I2SIN4_MCLK_EN_W_NAME "I2SIN4_MCLK_EN"
#define I2SIN6_MCLK_EN_W_NAME "I2SIN6_MCLK_EN"
#define I2SOUT0_MCLK_EN_W_NAME "I2SOUT0_MCLK_EN"
#define I2SOUT1_MCLK_EN_W_NAME "I2SOUT1_MCLK_EN"
#define I2SOUT2_MCLK_EN_W_NAME "I2SOUT2_MCLK_EN"
#define I2SOUT4_MCLK_EN_W_NAME "I2SOUT4_MCLK_EN"
#define I2SOUT6_MCLK_EN_W_NAME "I2SOUT6_MCLK_EN"
#define FMI2S_MASTER_MCLK_EN_W_NAME "FMI2S_MASTER_MCLK_EN"

static int get_i2s_id_by_name(struct mtk_base_afe *afe,
			      const char *name)
{
	if (strncmp(name, "I2SIN0", 6) == 0)
		return MT6989_DAI_I2S_IN0;
	else if (strncmp(name, "I2SIN1", 6) == 0)
		return MT6989_DAI_I2S_IN1;
	else if (strncmp(name, "I2SIN2", 6) == 0)
		return MT6989_DAI_I2S_IN2;
	else if (strncmp(name, "I2SIN4", 6) == 0)
		return MT6989_DAI_I2S_IN4;
	else if (strncmp(name, "I2SIN6", 6) == 0)
		return MT6989_DAI_I2S_IN6;
	else if (strncmp(name, "I2SOUT0", 7) == 0)
		return MT6989_DAI_I2S_OUT0;
	else if (strncmp(name, "I2SOUT1", 7) == 0)
		return MT6989_DAI_I2S_OUT1;
	else if (strncmp(name, "I2SOUT2", 7) == 0)
		return MT6989_DAI_I2S_OUT2;
	else if (strncmp(name, "I2SOUT4", 7) == 0)
		return MT6989_DAI_I2S_OUT4;
	else if (strncmp(name, "I2SOUT6", 7) == 0)
		return MT6989_DAI_I2S_OUT6;
	else if (strncmp(name, "FMI2S_MASTER", 12) == 0)
		return MT6989_DAI_FM_I2S_MASTER;
	else
		return -EINVAL;
}

static struct mtk_afe_i2s_priv *get_i2s_priv_by_name(struct mtk_base_afe *afe,
		const char *name)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int dai_id = get_i2s_id_by_name(afe, name);

	if (dai_id < 0)
		return NULL;

	return afe_priv->dai_priv[dai_id];
}

/*
 * bit mask for i2s low power control
 * such as bit0 for i2s0, bit1 for i2s1...
 * if set 1, means i2s low power mode
 * if set 0, means i2s low jitter mode
 * 0 for all i2s bit in default
 */
static unsigned int i2s_low_power_mask;
static int mtk_i2s_low_power_mask_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), mask: %x\n", __func__, i2s_low_power_mask);
	ucontrol->value.integer.value[0] = i2s_low_power_mask;
	return 0;
}

static int mtk_i2s_low_power_mask_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	i2s_low_power_mask = ucontrol->value.integer.value[0];
	pr_debug("%s(), mask: %x\n", __func__, i2s_low_power_mask);
	return 0;
}

static int mtk_is_i2s_low_power(int i2s_num)
{
	int i2s_bit_shift;

	i2s_bit_shift = i2s_num - MT6989_DAI_I2S_IN0;
	if (i2s_bit_shift < 0 || i2s_bit_shift > MT6989_DAI_I2S_MAX_NUM) {
		pr_debug("%s(), err i2s_num: %d\n", __func__, i2s_num);
		return 0;
	}
	return (i2s_low_power_mask>>i2s_bit_shift) & 0x1;
}

/* low jitter control */
static const char *const mt6989_i2s_hd_str[] = {
	"Normal", "Low_Jitter"
};

static const struct soc_enum mt6989_i2s_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_i2s_hd_str),
			    mt6989_i2s_hd_str),
};

static int mt6989_i2s_hd_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, kcontrol->id.name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = i2s_priv->low_jitter_en;

	return 0;
}

static int mt6989_i2s_hd_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int hd_en;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	hd_en = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, hd_en %d\n",
		 __func__, kcontrol->id.name, hd_en);

	i2s_priv = get_i2s_priv_by_name(afe, kcontrol->id.name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	i2s_priv->low_jitter_en = hd_en;

	return 0;
}

static const struct snd_kcontrol_new mtk_dai_i2s_controls[] = {
	SOC_ENUM_EXT(MTK_AFE_I2SIN0_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SIN1_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SIN2_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SIN4_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SIN6_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SOUT0_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SOUT1_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SOUT2_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SOUT4_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2SOUT6_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_FMI2S_MASTER_KCONTROL_NAME, mt6989_i2s_enum[0],
		     mt6989_i2s_hd_get, mt6989_i2s_hd_set),
	SOC_SINGLE_EXT("i2s_low_power_mask", SND_SOC_NOPM, 0, 0xffff, 0,
		       mtk_i2s_low_power_mask_get,
		       mtk_i2s_low_power_mask_set),

	SOC_ENUM_EXT("I2SIN0_LPBK", etdm_lpbk_map_enum,
		     etdm_lpbk_get, etdm_lpbk_put),
	SOC_ENUM_EXT("I2SIN1_LPBK", etdm_lpbk_map_enum,
		     etdm_lpbk_get, etdm_lpbk_put),
	SOC_ENUM_EXT("I2SIN2_LPBK", etdm_lpbk_map_enum,
		     etdm_lpbk_get, etdm_lpbk_put),
	SOC_ENUM_EXT("I2SIN4_LPBK", etdm_lpbk_map_enum,
		     etdm_lpbk_get, etdm_lpbk_put),
	SOC_ENUM_EXT("I2SIN6_LPBK", etdm_lpbk_map_enum,
		     etdm_lpbk_get, etdm_lpbk_put),
	SOC_ENUM_EXT("I2SIN4_IP_MODE", etdm_ip_mode_map_enum,
		     etdm_ip_mode_get, etdm_ip_mode_put),
	SOC_ENUM_EXT("I2SIN4_CH_NUM", etdm_ch_num_map_enum,
		     etdm_ch_num_get, etdm_ch_num_put),
	SOC_ENUM_EXT("I2SOUT4_CH_NUM", etdm_ch_num_map_enum,
		     etdm_ch_num_get, etdm_ch_num_put),
};

/* dai component */
/* i2s virtual mux to output widget */
static const char *const i2s_mux_map[] = {
	"Normal", "Dummy_Widget",
};

static int i2s_mux_map_value[] = {
	0, 1,
};

static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(i2s_mux_map_enum,
		SND_SOC_NOPM,
		0,
		1,
		i2s_mux_map,
		i2s_mux_map_value);

static const struct snd_kcontrol_new i2s_in0_mux_control =
	SOC_DAPM_ENUM("I2S IN0 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_in1_mux_control =
	SOC_DAPM_ENUM("I2S IN1 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_in2_mux_control =
	SOC_DAPM_ENUM("I2S IN2 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_in4_mux_control =
	SOC_DAPM_ENUM("I2S IN4 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_in6_mux_control =
	SOC_DAPM_ENUM("I2S IN6 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_out0_mux_control =
	SOC_DAPM_ENUM("I2S OUT0 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_out1_mux_control =
	SOC_DAPM_ENUM("I2S OUT1 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_out2_mux_control =
	SOC_DAPM_ENUM("I2S OUT2 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_out4_mux_control =
	SOC_DAPM_ENUM("I2S OUT4 Select", i2s_mux_map_enum);
static const struct snd_kcontrol_new i2s_out6_mux_control =
	SOC_DAPM_ENUM("I2S OUT6 Select", i2s_mux_map_enum);

/* interconnection */
static const struct snd_kcontrol_new mtk_i2sout0_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN108_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN108_1, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN108_1, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN108_1, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN108_1, I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN108_1, I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN108_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN108_1, I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH1", AFE_CONN108_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN108_1, I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN108_2, I_DL24_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN108_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN108_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN108_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN108_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN108_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN108_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN108_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout0_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN109_1, I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN109_1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN109_1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN109_1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN109_1, I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN109_1, I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN109_1, I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN109_1, I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH2", AFE_CONN109_1, I_DL8_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN109_1, I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN109_2, I_DL24_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN109_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN109_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN109_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN109_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN109_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN109_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN109_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN109_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN109_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN110_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN110_1, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN110_1, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN110_1, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN110_1, I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN110_1, I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN110_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN110_1, I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH1", AFE_CONN110_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN110_1, I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN110_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN110_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN110_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN110_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN111_1, I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN111_1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN111_1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN111_1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN111_1, I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN111_1, I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN111_1, I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN111_1, I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH2", AFE_CONN111_1, I_DL8_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN111_1, I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN111_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN111_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN111_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN111_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN111_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN111_4,
				    I_PCM_1_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN112_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN112_1, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN112_1, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN112_1, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN112_1, I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN112_1, I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN112_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN112_1, I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH1", AFE_CONN112_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN112_1, I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN112_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN112_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN112_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN112_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN113_1, I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN113_1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN113_1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN113_1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN113_1, I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN113_1, I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN113_1, I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN113_1, I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH2", AFE_CONN113_1, I_DL8_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN113_1, I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN113_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN113_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN113_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN113_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN113_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN113_4,
				    I_PCM_1_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN116_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN116_1, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN116_1, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN116_1, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN116_1, I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN116_1, I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN116_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN116_1, I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH1", AFE_CONN116_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN116_1, I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN116_2, I_DL24_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN116_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN116_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN116_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN116_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN116_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN116_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN116_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN116_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN117_1, I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN117_1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN117_1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN117_1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN117_1, I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN117_1, I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN117_1, I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN117_1, I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH2", AFE_CONN117_1, I_DL8_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN117_1, I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN117_2, I_DL24_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN117_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN117_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN117_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN117_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN117_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN117_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN117_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN117_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN117_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN117_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH3", AFE_CONN118_1, I_DL_24CH_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN118_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN118_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN118_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN118_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN118_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN118_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH4", AFE_CONN119_1, I_DL_24CH_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN119_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN119_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN119_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN119_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN119_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN119_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH5", AFE_CONN120_1, I_DL_24CH_CH5, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH6", AFE_CONN121_1, I_DL_24CH_CH6, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH7", AFE_CONN122_1, I_DL_24CH_CH7, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout4_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH8", AFE_CONN123_1, I_DL_24CH_CH8, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout6_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN148_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN148_1, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN148_1, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN148_1, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN148_1, I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN148_1, I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN148_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN148_1, I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH1", AFE_CONN148_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN148_2, I_DL23_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN148_1, I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN148_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN148_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN148_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN148_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN148_6,
				    I_SRC_1_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2sout6_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN149_1, I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN149_1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN149_1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN149_1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN149_1, I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN149_1, I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN149_1, I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN149_1, I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH2", AFE_CONN149_1, I_DL8_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN149_2, I_DL23_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN149_1, I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN149_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN149_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN149_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN149_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN149_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN149_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN148_6,
				    I_SRC_1_OUT_CH2, 1, 0),
};

enum {
	SUPPLY_SEQ_APLL,
	SUPPLY_SEQ_I2S_MCLK_EN,
	SUPPLY_SEQ_I2S_HD_EN,
	SUPPLY_SEQ_I2S_EN,
};

static int mtk_i2s_en_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	dev_info(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6989_afe_gpio_request(afe, true, i2s_priv->id, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6989_afe_gpio_request(afe, false, i2s_priv->id, 0);
		break;
	default:
		break;
	}
	switch (i2s_priv->id) {
	case MT6989_DAI_I2S_IN4:
		/* set etdm ch */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_CH_NUM_MASK_SFT, (i2s_priv->ch_num - 1) << REG_CH_NUM_SFT);
		/* set etdm ch */
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
				   REG_CH_NUM_MASK_SFT, (i2s_priv->ch_num - 1) << REG_CH_NUM_SFT);
		/* set etdm ip mode */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON2,
				   REG_MULTI_IP_MODE_MASK_SFT, i2s_priv->ip_mode << REG_MULTI_IP_MODE_SFT);
		/* set etdm sync */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_SYNC_MODE_MASK_SFT, i2s_priv->sync << REG_SYNC_MODE_SFT);
		break;
	case MT6989_DAI_I2S_IN0:
		/* set etdm sync */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_SYNC_MODE_MASK_SFT, 0x1 << REG_SYNC_MODE_SFT);
		break;
	case MT6989_DAI_I2S_IN1:
		/* set etdm sync */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_SYNC_MODE_MASK_SFT, 0x1 << REG_SYNC_MODE_SFT);
		break;
	case MT6989_DAI_I2S_IN2:
		/* set etdm sync */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_SYNC_MODE_MASK_SFT, 0x1 << REG_SYNC_MODE_SFT);
		break;
	case MT6989_DAI_I2S_IN6:
		/* set etdm sync */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_SYNC_MODE_MASK_SFT, 0x1 << REG_SYNC_MODE_SFT);
		break;

	default:
		break;
	}

	return 0;
}

static int mtk_i2s_hd_en_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);

	dev_dbg(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	return 0;
}

static int mtk_apll_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (strcmp(w->name, APLL1_W_NAME) == 0)
			mt6989_apll1_enable(afe);
		else
			mt6989_apll2_enable(afe);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (strcmp(w->name, APLL1_W_NAME) == 0)
			mt6989_apll1_disable(afe);
		else
			mt6989_apll2_disable(afe);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_mclk_en_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6989_mck_enable(afe, i2s_priv->mclk_id, i2s_priv->mclk_rate);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2s_priv->mclk_rate = 0;
		mt6989_mck_disable(afe, i2s_priv->mclk_id);
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget mtk_dai_i2s_widgets[] = {
	SND_SOC_DAPM_INPUT("CONNSYS"),

	SND_SOC_DAPM_MIXER("I2SOUT0_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout0_ch1_mix,
			   ARRAY_SIZE(mtk_i2sout0_ch1_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT0_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout0_ch2_mix,
			   ARRAY_SIZE(mtk_i2sout0_ch2_mix)),

	SND_SOC_DAPM_MIXER("I2SOUT1_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout1_ch1_mix,
			   ARRAY_SIZE(mtk_i2sout1_ch1_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT1_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout1_ch2_mix,
			   ARRAY_SIZE(mtk_i2sout1_ch2_mix)),

	SND_SOC_DAPM_MIXER("I2SOUT2_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout2_ch1_mix,
			   ARRAY_SIZE(mtk_i2sout2_ch1_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT2_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout2_ch2_mix,
			   ARRAY_SIZE(mtk_i2sout2_ch2_mix)),

	SND_SOC_DAPM_MIXER("I2SOUT4_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch1_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch1_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT4_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch2_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch2_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT4_CH3", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch3_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch3_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT4_CH4", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch4_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch4_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT4_CH5", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch5_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch5_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT4_CH6", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch6_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch6_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT4_CH7", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch7_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch7_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT4_CH8", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout4_ch8_mix,
			   ARRAY_SIZE(mtk_i2sout4_ch8_mix)),

	SND_SOC_DAPM_MIXER("I2SOUT6_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout6_ch1_mix,
			   ARRAY_SIZE(mtk_i2sout6_ch1_mix)),
	SND_SOC_DAPM_MIXER("I2SOUT6_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_i2sout6_ch2_mix,
			   ARRAY_SIZE(mtk_i2sout6_ch2_mix)),
	/* i2s gpio*/
	SND_SOC_DAPM_SUPPLY_S("I2SIN0_GPIO", SUPPLY_SEQ_I2S_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("I2SIN1_GPIO", SUPPLY_SEQ_I2S_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("I2SIN2_GPIO", SUPPLY_SEQ_I2S_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("I2SIN4_GPIO", SUPPLY_SEQ_I2S_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("I2SIN6_GPIO", SUPPLY_SEQ_I2S_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	/* i2s en*/
	SND_SOC_DAPM_SUPPLY_S("I2SIN0_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_IN0_CON0, REG_ETDM_IN_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SIN1_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_IN1_CON0, REG_ETDM_IN_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SIN2_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_IN2_CON0, REG_ETDM_IN_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SIN4_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_IN4_CON0, REG_ETDM_IN_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SIN6_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_IN6_CON0, REG_ETDM_IN_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SOUT0_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_OUT0_CON0, OUT_REG_ETDM_OUT_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SOUT1_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_OUT1_CON0, OUT_REG_ETDM_OUT_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SOUT2_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_OUT2_CON0, OUT_REG_ETDM_OUT_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SOUT4_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_OUT4_CON0, OUT_REG_ETDM_OUT_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2SOUT6_EN", SUPPLY_SEQ_I2S_EN,
			      ETDM_OUT6_CON0, OUT_REG_ETDM_OUT_EN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("FMI2S_MASTER_EN", SUPPLY_SEQ_I2S_EN,
			      AFE_CONNSYS_I2S_CON, I2S_EN_SFT, 0,
			      NULL, 0),

	/* i2s hd en */
	SND_SOC_DAPM_SUPPLY_S(I2SIN0_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN1_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN2_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN4_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN6_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT0_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT1_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT2_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT4_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT6_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(FMI2S_MASTER_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      AFE_CONNSYS_I2S_CON, I2S_HDEN_SFT, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* i2s mclk en */
	SND_SOC_DAPM_SUPPLY_S(I2SIN0_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN1_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN2_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN4_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SIN6_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT0_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT1_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT2_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT4_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2SOUT6_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(FMI2S_MASTER_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* apll */
	SND_SOC_DAPM_SUPPLY_S(APLL1_W_NAME, SUPPLY_SEQ_APLL,
			      SND_SOC_NOPM, 0, 0,
			      mtk_apll_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(APLL2_W_NAME, SUPPLY_SEQ_APLL,
			      SND_SOC_NOPM, 0, 0,
			      mtk_apll_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* allow i2s on without codec on */
	SND_SOC_DAPM_OUTPUT("I2S_DUMMY_OUT"),
	SND_SOC_DAPM_MUX("I2S_OUT0_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_out0_mux_control),
	SND_SOC_DAPM_MUX("I2S_OUT1_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_out1_mux_control),
	SND_SOC_DAPM_MUX("I2S_OUT2_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_out2_mux_control),
	SND_SOC_DAPM_MUX("I2S_OUT4_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_out4_mux_control),
	SND_SOC_DAPM_MUX("I2S_OUT6_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_out6_mux_control),

	SND_SOC_DAPM_INPUT("I2S_DUMMY_IN"),
	SND_SOC_DAPM_MUX("I2S_IN0_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_in0_mux_control),
	SND_SOC_DAPM_MUX("I2S_IN1_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_in1_mux_control),
	SND_SOC_DAPM_MUX("I2S_IN2_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_in2_mux_control),
	SND_SOC_DAPM_MUX("I2S_IN4_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_in4_mux_control),
	SND_SOC_DAPM_MUX("I2S_IN6_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s_in6_mux_control),
};

static int mtk_afe_i2s_share_connect(struct snd_soc_dapm_widget *source,
				     struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;
	int ret = 0;

	i2s_priv = get_i2s_priv_by_name(afe, sink->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	/*dev_dbg(afe->dev, "%s(), sink %s (id %d), share %d, source %s (id %d), ret = %d\n",
		 __func__,
		 sink->name,  get_i2s_id_by_name(afe, sink->name),
		 i2s_priv->share_i2s_id,
		 source->name, get_i2s_id_by_name(afe, source->name),
		 (i2s_priv->share_i2s_id == get_i2s_id_by_name(afe, source->name))? 1 : 0);
	*/

	if (i2s_priv->share_i2s_id < 0)
		return 0;

	ret = (i2s_priv->share_i2s_id == get_i2s_id_by_name(afe, source->name))? 1 : 0;

	return ret;
}

static int mtk_afe_i2s_hd_connect(struct snd_soc_dapm_widget *source,
				  struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;
	int i2s_num;


	i2s_priv = get_i2s_priv_by_name(afe, sink->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	i2s_num = get_i2s_id_by_name(afe, source->name);
	if (get_i2s_id_by_name(afe, sink->name) == i2s_num)
		return !mtk_is_i2s_low_power(i2s_num) ||
		       i2s_priv->low_jitter_en;

	/* check if share i2s need hd en */
	if (i2s_priv->share_i2s_id < 0)
		return 0;

	if (i2s_priv->share_i2s_id == i2s_num)
		return !mtk_is_i2s_low_power(i2s_num) ||
		       i2s_priv->low_jitter_en;

	return 0;
}

static int mtk_afe_i2s_apll_connect(struct snd_soc_dapm_widget *source,
				    struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;
	int cur_apll;
	int i2s_need_apll;

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	/* which apll */
	cur_apll = mt6989_get_apll_by_name(afe, source->name);

	/* choose APLL from i2s rate */
	i2s_need_apll = mt6989_get_apll_by_rate(afe, i2s_priv->rate);

	return (i2s_need_apll == cur_apll) ? 1 : 0;
}

static int mtk_afe_i2s_mclk_connect(struct snd_soc_dapm_widget *source,
				    struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, sink->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	if (get_i2s_id_by_name(afe, sink->name) ==
	    get_i2s_id_by_name(afe, source->name))
		return (i2s_priv->mclk_rate > 0) ? 1 : 0;

	/* check if share i2s need mclk */
	if (i2s_priv->share_i2s_id < 0)
		return 0;

	if (i2s_priv->share_i2s_id == get_i2s_id_by_name(afe, source->name))
		return (i2s_priv->mclk_rate > 0) ? 1 : 0;

	return 0;
}

static int mtk_afe_mclk_apll_connect(struct snd_soc_dapm_widget *source,
				     struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;
	int cur_apll;

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	/* which apll */
	cur_apll = mt6989_get_apll_by_name(afe, source->name);

	return (i2s_priv->mclk_apll == cur_apll) ? 1 : 0;
}

static const struct snd_soc_dapm_route mtk_dai_i2s_routes[] = {
	{"Connsys I2S", NULL, "CONNSYS"},

	/* i2sin0 */
	{"I2SIN0", NULL, "I2SIN0_GPIO"},
	{"I2SIN0", NULL, "I2SIN0_EN"},
	{"I2SIN0", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN0", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SIN0", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN0", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SIN0_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SIN0_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SIN0", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN0", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SIN0_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SIN0_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sin1 */
	{"I2SIN1", NULL, "I2SIN1_GPIO"},
	{"I2SIN1", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SIN1_EN"},
	{"I2SIN1", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN1", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SIN1", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN1", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SIN1_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SIN1_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SIN1", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN1", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SIN1_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SIN1_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sin2 */
	{"I2SIN2", NULL, "I2SIN2_GPIO"},
	{"I2SIN2", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SIN2_EN"},
	{"I2SIN2", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN2", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SIN2", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN2", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SIN2_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SIN2_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SIN2", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN2", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SIN2_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SIN2_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sin4 */
	{"I2SIN4", NULL, "I2SIN4_GPIO"},
	{"I2SIN4", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SIN4_EN"},
	{"I2SIN4", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN4", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SIN4", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN4", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SIN4_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SIN4_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SIN4", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN4", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SIN4_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SIN4_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sin6 */
	{"I2SIN6", NULL, "I2SIN6_GPIO"},
	{"I2SIN6", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SIN6_EN"},
	{"I2SIN6", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SIN6", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SIN6", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SIN6", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SIN6_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SIN6_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SIN6", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SIN6", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SIN6_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SIN6_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sout0 */
	{"I2SOUT0_CH1", "DL0_CH1", "DL0"},
	{"I2SOUT0_CH2", "DL0_CH2", "DL0"},
	{"I2SOUT0_CH1", "DL1_CH1", "DL1"},
	{"I2SOUT0_CH2", "DL1_CH2", "DL1"},
	{"I2SOUT0_CH1", "DL2_CH1", "DL2"},
	{"I2SOUT0_CH2", "DL2_CH2", "DL2"},
	{"I2SOUT0_CH1", "DL3_CH1", "DL3"},
	{"I2SOUT0_CH2", "DL3_CH2", "DL3"},
	{"I2SOUT0_CH1", "DL4_CH1", "DL4"},
	{"I2SOUT0_CH2", "DL4_CH2", "DL4"},
	{"I2SOUT0_CH1", "DL5_CH1", "DL5"},
	{"I2SOUT0_CH2", "DL5_CH2", "DL5"},
	{"I2SOUT0_CH1", "DL6_CH1", "DL6"},
	{"I2SOUT0_CH2", "DL6_CH2", "DL6"},
	{"I2SOUT0_CH1", "DL7_CH1", "DL7"},
	{"I2SOUT0_CH2", "DL7_CH2", "DL7"},
	{"I2SOUT0_CH1", "DL8_CH1", "DL8"},
	{"I2SOUT0_CH2", "DL8_CH2", "DL8"},
	{"I2SOUT0_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"I2SOUT0_CH2", "DL_24CH_CH2", "DL_24CH"},

	{"I2SOUT0_CH1", "DL24_CH1", "DL24"},
	{"I2SOUT0_CH2", "DL24_CH2", "DL24"},

	{"I2SOUT0", NULL, "I2SOUT0_CH1"},
	{"I2SOUT0", NULL, "I2SOUT0_CH2"},

	{"I2SOUT0", NULL, "I2SIN0_GPIO"},
	{"I2SOUT0", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SOUT0_EN"},
	{"I2SOUT0", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT0", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SOUT0", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT0", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SOUT0_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SOUT0_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SOUT0", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT0", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SOUT0_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SOUT0_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sout1 */
	{"I2SOUT1_CH1", "DL0_CH1", "DL0"},
	{"I2SOUT1_CH2", "DL0_CH2", "DL0"},
	{"I2SOUT1_CH1", "DL1_CH1", "DL1"},
	{"I2SOUT1_CH2", "DL1_CH2", "DL1"},
	{"I2SOUT1_CH1", "DL2_CH1", "DL2"},
	{"I2SOUT1_CH2", "DL2_CH2", "DL2"},
	{"I2SOUT1_CH1", "DL3_CH1", "DL3"},
	{"I2SOUT1_CH2", "DL3_CH2", "DL3"},
	{"I2SOUT1_CH1", "DL4_CH1", "DL4"},
	{"I2SOUT1_CH2", "DL4_CH2", "DL4"},
	{"I2SOUT1_CH1", "DL5_CH1", "DL5"},
	{"I2SOUT1_CH2", "DL5_CH2", "DL5"},
	{"I2SOUT1_CH1", "DL6_CH1", "DL6"},
	{"I2SOUT1_CH2", "DL6_CH2", "DL6"},
	{"I2SOUT1_CH1", "DL7_CH1", "DL7"},
	{"I2SOUT1_CH2", "DL7_CH2", "DL7"},
	{"I2SOUT1_CH1", "DL8_CH1", "DL8"},
	{"I2SOUT1_CH2", "DL8_CH2", "DL8"},
	{"I2SOUT1_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"I2SOUT1_CH2", "DL_24CH_CH2", "DL_24CH"},

	{"I2SOUT1", NULL, "I2SOUT1_CH1"},
	{"I2SOUT1", NULL, "I2SOUT1_CH2"},

	{"I2SOUT1", NULL, "I2SIN1_GPIO"},
	{"I2SOUT1", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SOUT1_EN"},
	{"I2SOUT1", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT1", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SOUT1", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT1", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SOUT1_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SOUT1_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SOUT1", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT1", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SOUT1_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SOUT1_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sout2 */
	{"I2SOUT2_CH1", "DL0_CH1", "DL0"},
	{"I2SOUT2_CH2", "DL0_CH2", "DL0"},
	{"I2SOUT2_CH1", "DL1_CH1", "DL1"},
	{"I2SOUT2_CH2", "DL1_CH2", "DL1"},
	{"I2SOUT2_CH1", "DL2_CH1", "DL2"},
	{"I2SOUT2_CH2", "DL2_CH2", "DL2"},
	{"I2SOUT2_CH1", "DL3_CH1", "DL3"},
	{"I2SOUT2_CH2", "DL3_CH2", "DL3"},
	{"I2SOUT2_CH1", "DL4_CH1", "DL4"},
	{"I2SOUT2_CH2", "DL4_CH2", "DL4"},
	{"I2SOUT2_CH1", "DL5_CH1", "DL5"},
	{"I2SOUT2_CH2", "DL5_CH2", "DL5"},
	{"I2SOUT2_CH1", "DL6_CH1", "DL6"},
	{"I2SOUT2_CH2", "DL6_CH2", "DL6"},
	{"I2SOUT2_CH1", "DL7_CH1", "DL7"},
	{"I2SOUT2_CH2", "DL7_CH2", "DL7"},
	{"I2SOUT2_CH1", "DL8_CH1", "DL8"},
	{"I2SOUT2_CH2", "DL8_CH2", "DL8"},
	{"I2SOUT2_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"I2SOUT2_CH2", "DL_24CH_CH2", "DL_24CH"},

	{"I2SOUT2", NULL, "I2SOUT2_CH1"},
	{"I2SOUT2", NULL, "I2SOUT2_CH2"},

	{"I2SOUT2", NULL, "I2SIN2_GPIO"},
	{"I2SOUT2", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SOUT2_EN"},
	{"I2SOUT2", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT2", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SOUT2", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT2", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SOUT2_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SOUT2_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SOUT2", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT2", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SOUT2_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SOUT2_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sout4 */
	{"I2SOUT4_CH1", "DL0_CH1", "DL0"},
	{"I2SOUT4_CH2", "DL0_CH2", "DL0"},
	{"I2SOUT4_CH1", "DL1_CH1", "DL1"},
	{"I2SOUT4_CH2", "DL1_CH2", "DL1"},
	{"I2SOUT4_CH1", "DL2_CH1", "DL2"},
	{"I2SOUT4_CH2", "DL2_CH2", "DL2"},
	{"I2SOUT4_CH1", "DL3_CH1", "DL3"},
	{"I2SOUT4_CH2", "DL3_CH2", "DL3"},
	{"I2SOUT4_CH1", "DL4_CH1", "DL4"},
	{"I2SOUT4_CH2", "DL4_CH2", "DL4"},
	{"I2SOUT4_CH1", "DL5_CH1", "DL5"},
	{"I2SOUT4_CH2", "DL5_CH2", "DL5"},
	{"I2SOUT4_CH1", "DL6_CH1", "DL6"},
	{"I2SOUT4_CH2", "DL6_CH2", "DL6"},
	{"I2SOUT4_CH1", "DL7_CH1", "DL7"},
	{"I2SOUT4_CH2", "DL7_CH2", "DL7"},
	{"I2SOUT4_CH1", "DL8_CH1", "DL8"},
	{"I2SOUT4_CH2", "DL8_CH2", "DL8"},
	{"I2SOUT4_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"I2SOUT4_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"I2SOUT4_CH3", "DL_24CH_CH3", "DL_24CH"},
	{"I2SOUT4_CH4", "DL_24CH_CH4", "DL_24CH"},
	{"I2SOUT4_CH5", "DL_24CH_CH5", "DL_24CH"},
	{"I2SOUT4_CH6", "DL_24CH_CH6", "DL_24CH"},
	{"I2SOUT4_CH7", "DL_24CH_CH7", "DL_24CH"},
	{"I2SOUT4_CH8", "DL_24CH_CH8", "DL_24CH"},
	{"I2SOUT4_CH1", "DL24_CH1", "DL24"},
	{"I2SOUT4_CH2", "DL24_CH2", "DL24"},

	{"I2SOUT4", NULL, "I2SOUT4_CH1"},
	{"I2SOUT4", NULL, "I2SOUT4_CH2"},
	{"I2SOUT4", NULL, "I2SOUT4_CH3"},
	{"I2SOUT4", NULL, "I2SOUT4_CH4"},
	{"I2SOUT4", NULL, "I2SOUT4_CH5"},
	{"I2SOUT4", NULL, "I2SOUT4_CH6"},
	{"I2SOUT4", NULL, "I2SOUT4_CH7"},
	{"I2SOUT4", NULL, "I2SOUT4_CH8"},


	{"I2SOUT4", NULL, "I2SIN4_GPIO"},
	{"I2SOUT4", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "I2SOUT4_EN"},
	{"I2SOUT4", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT4", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SOUT4", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT4", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SOUT4_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SOUT4_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SOUT4", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT4", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SOUT4_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SOUT4_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2sout6 */
	{"I2SOUT6_CH1", "DL0_CH1", "DL0"},
	{"I2SOUT6_CH2", "DL0_CH2", "DL0"},
	{"I2SOUT6_CH1", "DL1_CH1", "DL1"},
	{"I2SOUT6_CH2", "DL1_CH2", "DL1"},
	{"I2SOUT6_CH1", "DL2_CH1", "DL2"},
	{"I2SOUT6_CH2", "DL2_CH2", "DL2"},
	{"I2SOUT6_CH1", "DL3_CH1", "DL3"},
	{"I2SOUT6_CH2", "DL3_CH2", "DL3"},
	{"I2SOUT6_CH1", "DL4_CH1", "DL4"},
	{"I2SOUT6_CH2", "DL4_CH2", "DL4"},
	{"I2SOUT6_CH1", "DL5_CH1", "DL5"},
	{"I2SOUT6_CH2", "DL5_CH2", "DL5"},
	{"I2SOUT6_CH1", "DL6_CH1", "DL6"},
	{"I2SOUT6_CH2", "DL6_CH2", "DL6"},
	{"I2SOUT6_CH1", "DL7_CH1", "DL7"},
	{"I2SOUT6_CH2", "DL7_CH2", "DL7"},
	{"I2SOUT6_CH1", "DL8_CH1", "DL8"},
	{"I2SOUT6_CH2", "DL8_CH2", "DL8"},
	{"I2SOUT6_CH1", "DL23_CH1", "DL23"},
	{"I2SOUT6_CH2", "DL23_CH2", "DL23"},
	{"I2SOUT6_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"I2SOUT6_CH2", "DL_24CH_CH2", "DL_24CH"},

	{"I2SOUT6", NULL, "I2SOUT6_CH1"},
	{"I2SOUT6", NULL, "I2SOUT6_CH2"},

	{"I2SOUT6", NULL, "I2SIN6_GPIO"},
	{"I2SOUT6", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"I2SOUT6", NULL, "I2SOUT6_EN"},
	{"I2SOUT6", NULL, "FMI2S_MASTER_EN", mtk_afe_i2s_share_connect},

	{"I2SOUT6", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2SOUT6", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2SOUT6_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2SOUT6_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2SOUT6", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2SOUT6", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2SOUT6_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2SOUT6_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* fmi2s */
	{"FMI2S_MASTER", NULL, "I2SIN0_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SIN1_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SIN2_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SIN4_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SIN6_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SOUT0_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SOUT1_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SOUT2_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SOUT4_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "I2SOUT6_EN", mtk_afe_i2s_share_connect},
	{"FMI2S_MASTER", NULL, "FMI2S_MASTER_EN"},

	{"FMI2S_MASTER", NULL, I2SIN0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SIN1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SIN2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SIN4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SIN6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SOUT0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SOUT1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SOUT2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SOUT4_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, I2SOUT6_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"FMI2S_MASTER", NULL, FMI2S_MASTER_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{FMI2S_MASTER_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{FMI2S_MASTER_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"FMI2S_MASTER", NULL, I2SIN0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SIN1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SIN2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SIN4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SIN6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SOUT0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SOUT1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SOUT2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SOUT4_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, I2SOUT6_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"FMI2S_MASTER", NULL, FMI2S_MASTER_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{FMI2S_MASTER_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{FMI2S_MASTER_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* allow i2s on without codec on */
	{"I2SIN0", NULL, "I2S_IN0_Mux"},
	{"I2S_IN0_Mux", "Dummy_Widget", "I2S_DUMMY_IN"},

	{"I2SIN1", NULL, "I2S_IN1_Mux"},
	{"I2S_IN1_Mux", "Dummy_Widget", "I2S_DUMMY_IN"},

	{"I2SIN2", NULL, "I2S_IN2_Mux"},
	{"I2S_IN2_Mux", "Dummy_Widget", "I2S_DUMMY_IN"},

	{"I2SIN4", NULL, "I2S_IN4_Mux"},
	{"I2S_IN4_Mux", "Dummy_Widget", "I2S_DUMMY_IN"},

	{"I2SIN6", NULL, "I2S_IN6_Mux"},
	{"I2S_IN6_Mux", "Dummy_Widget", "I2S_DUMMY_IN"},

	{"I2S_OUT0_Mux", "Dummy_Widget", "I2SOUT0"},
	{"I2S_DUMMY_OUT", NULL, "I2S_OUT0_Mux"},

	{"I2S_OUT1_Mux", "Dummy_Widget", "I2SOUT1"},
	{"I2S_DUMMY_OUT", NULL, "I2S_OUT1_Mux"},

	{"I2S_OUT2_Mux", "Dummy_Widget", "I2SOUT2"},
	{"I2S_DUMMY_OUT", NULL, "I2S_OUT2_Mux"},

	{"I2S_OUT4_Mux", "Dummy_Widget", "I2SOUT4"},
	{"I2S_DUMMY_OUT", NULL, "I2S_OUT4_Mux"},

	{"I2S_OUT6_Mux", "Dummy_Widget", "I2SOUT6"},
	{"I2S_DUMMY_OUT", NULL, "I2S_OUT6_Mux"},
};

/* dai ops */
#define SRC_REG
static int mtk_dai_connsys_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int rate = params_rate(params);
	unsigned int rate_reg = mt6989_rate_transform(afe->dev,
				rate, dai->id);
	unsigned int i2s_con = 0;

	dev_info(afe->dev, "%s(), id %d, stream %d, rate %d\n",
		 __func__,
		 dai->id,
		 substream->stream,
		 rate);

	/* non-inverse, i2s mode, slave, 16bits, from connsys */
	i2s_con |= I2S_FMT_I2S << I2S_FMT_SFT;
	i2s_con |= 1 << I2S_SRC_SFT;
	i2s_con |= get_i2s_wlen(SNDRV_PCM_FORMAT_S16_LE) << I2S_WLEN_SFT;
	i2s_con |= 0 << I2SIN_PAD_SEL_SFT;
	regmap_write(afe->regmap, AFE_CONNSYS_I2S_CON, i2s_con);

	/* use asrc */
	regmap_update_bits(afe->regmap,
			   AFE_CONNSYS_I2S_CON,
			   I2S_BYPSRC_MASK_SFT,
			   0x0 << I2S_BYPSRC_SFT);

	/* slave mode, set i2s for asrc */
	regmap_update_bits(afe->regmap,
			   AFE_CONNSYS_I2S_CON,
			   I2S_MODE_MASK_SFT,
			   rate_reg << I2S_MODE_SFT);
#ifdef SRC_REG
#if !defined(FM_SRC_CAIL)
	if (rate == 44100)
		regmap_write(afe->regmap, AFE_ASRC_NEW_CON3, 0x001B9000);
	else if (rate == 32000)
		regmap_write(afe->regmap, AFE_ASRC_NEW_CON3, 0x140000);
	else
		regmap_write(afe->regmap, AFE_ASRC_NEW_CON3, 0x001E0000);

	/* Calibration setting */
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON4, 0x00140000);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON13, 0x00036000);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON14, 0x0002FC00);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON7, 0x00007EF4);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON6, 0x00FF5986);
#else
	if (rate == 44100)
		regmap_write(afe->regmap, AFE_ASRC_NEW_CON2, 0x001b9000);
	else if (rate == 32000)
		regmap_write(afe->regmap, AFE_ASRC_NEW_CON2, 0x140000);
	else
		regmap_write(afe->regmap, AFE_ASRC_NEW_CON2, 0x000f0000);

	/* Calibration setting */
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON3, 0x000a0000);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON13, 0x001b000);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON14, 0x0017c00);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON7, 0x00001fbd);
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON5, 0xa000000);

#if !defined(FM_SRC_CAIL)
	regmap_update_bits(afe->regmap,
			   AFE_ASRC_NEW_CON0,
			   CHSET0_IFS_SEL_MASK_SFT,
			   0x3 << CHSET0_IFS_SEL_SFT);
	regmap_update_bits(afe->regmap,
			   AFE_ASRC_NEW_CON0,
			   CHSET0_OFS_SEL_MASK_SFT,
			   0x2 << CHSET0_OFS_SEL_SFT);
#else
	regmap_update_bits(afe->regmap,
			   AFE_ASRC_NEW_CON0,
			   CHSET0_IFS_SEL_MASK_SFT,
			   0x2 << CHSET0_IFS_SEL_SFT);
	regmap_update_bits(afe->regmap,
			   AFE_ASRC_NEW_CON0,
			   CHSET0_OFS_SEL_MASK_SFT,
			   0x1 << CHSET0_OFS_SEL_SFT);
#endif
	regmap_write(afe->regmap, AFE_ASRC_NEW_CON6, 0x7f888e);
#endif

	/* 0:Stereo 1:Mono */
	regmap_update_bits(afe->regmap,
			   AFE_ASRC_NEW_CON0,
			   CHSET0_IS_MONO_MASK_SFT,
			   0x0 << CHSET0_IS_MONO_SFT);
#endif

	return 0;
}

static int mtk_dai_connsys_i2s_trigger(struct snd_pcm_substream *substream,
				       int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s(), cmd %d, stream %d\n",
		 __func__,
		 cmd,
		 substream->stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* i2s enable */
		regmap_update_bits(afe->regmap,
				   AFE_CONNSYS_I2S_CON,
				   I2S_EN_MASK_SFT,
				   0x1 << I2S_EN_SFT);
#ifdef SRC_REG

		/* calibrator enable */
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_NEW_CON6,
				   CALI_EN_MASK_SFT,
				   0x1 << CALI_EN_SFT);

		/* asrc enable */
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_NEW_CON0,
				   CHSET_STR_CLR_MASK_SFT,
				   0x1 << CHSET_STR_CLR_SFT);
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_NEW_CON0,
				   ASM_ON_MASK_SFT,
				   0x1 << ASM_ON_SFT);
#endif
		afe_priv->dai_on[dai->id] = true;
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
#ifdef SRC_REG
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_NEW_CON0,
				   ASM_ON_MASK_SFT,
				   0 << ASM_ON_SFT);
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_NEW_CON6,
				   CALI_EN_MASK_SFT,
				   0 << CALI_EN_SFT);
#endif
		/* i2s disable */
		regmap_update_bits(afe->regmap,
				   AFE_CONNSYS_I2S_CON,
				   I2S_EN_MASK_SFT,
				   0x0 << I2S_EN_SFT);

		/* bypass asrc */
		regmap_update_bits(afe->regmap,
				   AFE_CONNSYS_I2S_CON,
				   I2S_BYPSRC_MASK_SFT,
				   0x1 << I2S_BYPSRC_SFT);

		afe_priv->dai_on[dai->id] = false;
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_connsys_i2s_ops = {
	.hw_params = mtk_dai_connsys_i2s_hw_params,
	.trigger = mtk_dai_connsys_i2s_trigger,
};

/* etdm dai ops */
static int mtk_dai_etdm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int i2s_id = dai->id;
	struct mtk_afe_i2s_priv *i2s_priv = afe_priv->dai_priv[i2s_id];
	unsigned int rate = params_rate(params);
	unsigned int channels = params_channels(params);
	snd_pcm_format_t format = params_format(params);

	dev_info(afe->dev, "%s(), %s(%d), stream %d, rate %d channels %d format %d\n",
		 __func__, dai->name, i2s_id, substream->stream, rate, channels, format);

	i2s_priv->rate = rate;

	/* ETDM_IN Supports even channel only */
	if ((channels % 2) != 0)
		dev_info(afe->dev, "%s(), channels(%d) not even\n", __func__, channels);

	switch (i2s_id) {
	case MT6989_DAI_I2S_IN0:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN0_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
				   ETDM_IN0_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT0_MASTER
				   << ETDM_IN0_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN1:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN1_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON1,
				   ETDM_IN1_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT1_MASTER
				   << ETDM_IN1_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN2:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN2_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON2,
				   ETDM_IN2_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT2_MASTER
				   << ETDM_IN2_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN4:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x3 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN4_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 4: TDM (DSP_A) Mode */
		if (i2s_priv->ch_num > 2)
			regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
					   REG_FMT_MASK_SFT, 0x4 << REG_FMT_SFT);
		else
			regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
					   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON0,
				   ETDM_IN4_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT4_MASTER
				   << ETDM_IN4_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN6:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN6_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON2,
				   ETDM_IN6_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT6_MASTER
				   << ETDM_IN6_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT0:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
			   ETDM_OUT0_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN0_MASTER
			   << ETDM_OUT0_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT1:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
			   ETDM_OUT1_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN1_MASTER
			   << ETDM_OUT1_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT2:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON2,
			   ETDM_OUT2_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN2_MASTER
			   << ETDM_OUT2_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT4:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 4: TDM (DSP_A) Mode */
		if (i2s_priv->ch_num > 2)
			regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
					   OUT_REG_FMT_MASK_SFT, 0x4 << OUT_REG_FMT_SFT);
		else
			regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
					   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON0,
			   ETDM_OUT4_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN4_MASTER
			   << ETDM_OUT4_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT6:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON2,
			   ETDM_OUT6_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN6_MASTER
			   << ETDM_OUT6_SLAVE_SEL_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_dai_etdm_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	dev_info(afe->dev, "%s(), %s(%d): cmd %d\n", __func__, dai->name, dai->id, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* enable etdm in/out */
		switch (dai->id) {
		case MT6989_DAI_I2S_IN0:
			regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x1 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN1:
			regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x1 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN2:
			regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x1 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN4:
			regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x1 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN6:
			regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x1 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT0:
			regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x1 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT1:
			regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x1 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT2:
			regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x1 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT4:
			regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x1 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT6:
			regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x1 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		default:
			return -EINVAL;
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* disable etdm in/out */
		switch (dai->id) {
		case MT6989_DAI_I2S_IN0:
			regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x0 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN1:
			regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x0 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN2:
			regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x0 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN4:
			regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x0 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_IN6:
			regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x0 << REG_ETDM_IN_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT0:
			regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x0 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT1:
			regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x0 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT2:
			regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x0 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT4:
			regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x0 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		case MT6989_DAI_I2S_OUT6:
			regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
					   OUT_REG_ETDM_OUT_EN_MASK_SFT,
					   0x0 << OUT_REG_ETDM_OUT_EN_SFT);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_etdm_ops = {
	.hw_params = mtk_dai_etdm_hw_params,
	.trigger = mtk_dai_etdm_trigger,
};

/* i2s dai ops*/
static int mtk_dai_i2s_config(struct mtk_base_afe *afe,
			      struct snd_pcm_hw_params *params,
			      int i2s_id)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2s_priv = afe_priv->dai_priv[i2s_id];

	unsigned int rate = params_rate(params);
	unsigned int rate_reg = mt6989_rate_transform(afe->dev,
				rate, i2s_id);
	snd_pcm_format_t format = params_format(params);
	unsigned int i2s_con = 0;
	int ret = 0;

	dev_info(afe->dev, "%s(), id %d, rate %d, format %d, ch %d\n",
		 __func__,
		 i2s_id,
		 rate, format, i2s_priv->ch_num);

	if (i2s_priv)
		i2s_priv->rate = rate;
	else
		AUDIO_AEE("i2s_priv == NULL");

	switch (i2s_id) {
	case MT6989_DAI_FM_I2S_MASTER:
		i2s_con = I2S_IN_PAD_IO_MUX << I2SIN_PAD_SEL_SFT;
		i2s_con |= rate_reg << I2S_MODE_SFT;
		i2s_con |= I2S_FMT_I2S << I2S_FMT_SFT;
		i2s_con |= get_i2s_wlen(format) << I2S_WLEN_SFT;
		regmap_update_bits(afe->regmap, AFE_CONNSYS_I2S_CON,
				   0xffffeffe, i2s_con);
		break;
	case MT6989_DAI_I2S_IN0:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN0_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN0_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
				   ETDM_IN0_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT0_MASTER
				   << ETDM_IN0_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN1:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN1_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON1,
				   ETDM_IN1_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT1_MASTER
				   << ETDM_IN1_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN2:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN2_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON2,
				   ETDM_IN2_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT2_MASTER
				   << ETDM_IN2_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN4:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x3 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN4_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 4: TDM (DSP_A) Mode */
		if (i2s_priv->ch_num > 2)
			regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
					   REG_FMT_MASK_SFT, 0x4 << REG_FMT_SFT);
		else
			regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
					   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN4_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON0,
				   ETDM_IN4_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT4_MASTER
				   << ETDM_IN4_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_IN6:
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN6_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON9,
				   REG_OUT2LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_FMT_MASK_SFT, 0x0 << REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_RELATCH_1X_EN_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN6_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON2,
				   ETDM_IN6_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT6_MASTER
				   << ETDM_IN6_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT0:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT0_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
			   ETDM_OUT0_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN0_MASTER
			   << ETDM_OUT0_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT1:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
			   ETDM_OUT1_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN1_MASTER
			   << ETDM_OUT1_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT2:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON2,
			   ETDM_OUT2_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN2_MASTER
			   << ETDM_OUT2_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT4:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 4: TDM (DSP_A) Mode */
		if (i2s_priv->ch_num > 2)
			regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
					   OUT_REG_FMT_MASK_SFT, 0x4 << OUT_REG_FMT_SFT);
		else
			regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
					   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);


		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT4_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON0,
			   ETDM_OUT4_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN4_MASTER
			   << ETDM_OUT4_SLAVE_SEL_SFT);
		break;
	case MT6989_DAI_I2S_OUT6:
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON1,
				   OUT_REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << OUT_REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON1,
				   OUT_REG_INITIAL_POINT_MASK_SFT,
				   0x6 << OUT_REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON1,
				   OUT_REG_LRCK_RESET_MASK_SFT,
				   0x1 << OUT_REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON4,
				   OUT_REG_RELATCH_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << OUT_REG_RELATCH_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_FMT_MASK_SFT, 0x0 << OUT_REG_FMT_SFT);

		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << OUT_REG_RELATCH_DOMAIN_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << OUT_REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT6_CON0,
				   OUT_REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << OUT_REG_WORD_LENGTH_SFT);

		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON2,
			   ETDM_OUT6_SLAVE_SEL_MASK_SFT,
			   ETDM_SLAVE_SEL_ETDMIN6_MASTER
			   << ETDM_OUT6_SLAVE_SEL_SFT);
		break;
	default:
		dev_info(afe->dev, "%s(), id %d not support\n",
			 __func__, i2s_id);
		return -EINVAL;
	}

	/* set share i2s */
	if (i2s_priv && i2s_priv->share_i2s_id >= 0)
		ret = mtk_dai_i2s_config(afe, params, i2s_priv->share_i2s_id);

	return ret;
}

static int mtk_dai_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	return mtk_dai_i2s_config(afe, params, dai->id);
}

static int mtk_dai_i2s_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dai->dev);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2s_priv = afe_priv->dai_priv[dai->id];
	int apll;
	int apll_rate;

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	if (dir != SND_SOC_CLOCK_OUT) {
		AUDIO_AEE("dir != SND_SOC_CLOCK_OUT");
		return -EINVAL;
	}

	apll = mt6989_get_apll_by_rate(afe, freq);
	apll_rate = mt6989_get_apll_rate(afe, apll);

	if (freq > apll_rate) {
		AUDIO_AEE("freq > apll rate");
		return -EINVAL;
	}

	if (apll_rate % freq != 0) {
		AUDIO_AEE("APLL cannot generate freq Hz");
		return -EINVAL;
	}

	i2s_priv->mclk_rate = freq;
	i2s_priv->mclk_apll = apll;

	if (i2s_priv->share_i2s_id > 0) {
		struct mtk_afe_i2s_priv *share_i2s_priv;

		share_i2s_priv = afe_priv->dai_priv[i2s_priv->share_i2s_id];
		if (!share_i2s_priv) {
			AUDIO_AEE("share_i2s_priv == NULL");
			return -EINVAL;
		}

		share_i2s_priv->mclk_rate = i2s_priv->mclk_rate;
		share_i2s_priv->mclk_apll = i2s_priv->mclk_apll;
	}

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_i2s_ops = {
	.hw_params = mtk_dai_i2s_hw_params,
	.set_sysclk = mtk_dai_i2s_set_sysclk,
};

/* dai driver */
#define MTK_CONNSYS_I2S_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

#define MTK_ETDM_RATES (SNDRV_PCM_RATE_8000_384000)
#define MTK_ETDM_FORMATS (SNDRV_PCM_FMTBIT_S8 |\
			  SNDRV_PCM_FMTBIT_S16_LE |\
			  SNDRV_PCM_FMTBIT_S24_LE |\
			  SNDRV_PCM_FMTBIT_S32_LE)

#define MTK_I2S_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)
#define MTK_I2S_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_i2s_driver[] = {
	{
		.name = "CONNSYS_I2S",
		.id = MT6989_DAI_CONNSYS_I2S,
		.capture = {
			.stream_name = "Connsys I2S",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_CONNSYS_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_connsys_i2s_ops,
	},
	{
		.name = "I2SIN0",
		.id = MT6989_DAI_I2S_IN0,
		.capture = {
			.stream_name = "I2SIN0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SIN1",
		.id = MT6989_DAI_I2S_IN1,
		.capture = {
			.stream_name = "I2SIN1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SIN2",
		.id = MT6989_DAI_I2S_IN2,
		.capture = {
			.stream_name = "I2SIN2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SIN4",
		.id = MT6989_DAI_I2S_IN4,
		.capture = {
			.stream_name = "I2SIN4",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SIN6",
		.id = MT6989_DAI_I2S_IN6,
		.capture = {
			.stream_name = "I2SIN6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SOUT0",
		.id = MT6989_DAI_I2S_OUT0,
		.playback = {
			.stream_name = "I2SOUT0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SOUT1",
		.id = MT6989_DAI_I2S_OUT1,
		.playback = {
			.stream_name = "I2SOUT1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SOUT2",
		.id = MT6989_DAI_I2S_OUT2,
		.playback = {
			.stream_name = "I2SOUT2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SOUT4",
		.id = MT6989_DAI_I2S_OUT4,
		.playback = {
			.stream_name = "I2SOUT4",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2SOUT6",
		.id = MT6989_DAI_I2S_OUT6,
		.playback = {
			.stream_name = "I2SOUT6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "FMI2S_MASTER",
		.id = MT6989_DAI_FM_I2S_MASTER,
		.capture = {
			.stream_name = "FMI2S_MASTER",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
};

/* this enum is merely for mtk_afe_i2s_priv declare */
enum {
	DAI_I2SIN0 = 0,
	DAI_I2SIN1,
	DAI_I2SIN2,
	DAI_I2SIN4,
	DAI_I2SIN6,
	DAI_I2SOUT0,
	DAI_I2SOUT1,
	DAI_I2SOUT2,
	DAI_I2SOUT4,
	DAI_I2SOUT6,
	DAI_FMI2S_MASTER,
	DAI_I2S_NUM,
};

static const struct mtk_afe_i2s_priv mt6989_i2s_priv[DAI_I2S_NUM] = {
	[DAI_I2SIN0] = {
		.id = MT6989_DAI_I2S_IN0,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sin0-share",
		.share_i2s_id = -1,
	},
	[DAI_I2SIN1] = {
		.id = MT6989_DAI_I2S_IN1,
		.mclk_id = MT6989_I2SIN1_MCK,
		.share_property_name = "i2sin1-share",
		.share_i2s_id = -1,
	},
	[DAI_I2SIN2] = {
		.id = MT6989_DAI_I2S_IN2,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sin2-share",
		.share_i2s_id = -1,
	},
	[DAI_I2SIN4] = {
		.id = MT6989_DAI_I2S_IN4,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sin4-share",
		.share_i2s_id = -1,
	},
	[DAI_I2SIN6] = {
		.id = MT6989_DAI_I2S_IN6,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sout6-share",
		.share_i2s_id = -1,
	},
	[DAI_I2SOUT0] = {
		.id = MT6989_DAI_I2S_OUT0,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sout0-share",
		.share_i2s_id = MT6989_DAI_I2S_IN0,
	},
	[DAI_I2SOUT1] = {
		.id = MT6989_DAI_I2S_OUT1,
		.mclk_id = MT6989_I2SIN1_MCK,
		.share_property_name = "i2sout1-share",
		.share_i2s_id = MT6989_DAI_I2S_IN1,
	},
	[DAI_I2SOUT2] = {
		.id = MT6989_DAI_I2S_OUT2,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sout2-share",
		.share_i2s_id = MT6989_DAI_I2S_IN2,
	},
	[DAI_I2SOUT4] = {
		.id = MT6989_DAI_I2S_OUT4,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sout4-share",
		.share_i2s_id = MT6989_DAI_I2S_IN4,
	},
	[DAI_I2SOUT6] = {
		.id = MT6989_DAI_I2S_OUT6,
		.mclk_id = MT6989_I2SIN0_MCK,
		.share_property_name = "i2sout6-share",
		.share_i2s_id = MT6989_DAI_I2S_IN6,
	},
	[DAI_FMI2S_MASTER] = {
		.id = MT6989_DAI_FM_I2S_MASTER,
		.mclk_id = MT6989_FMI2S_MCK,
		.share_property_name = "fmi2s-share",
		.share_i2s_id = -1,
	},
};
static int etdm_parse_dt(struct mtk_base_afe *afe)
{
	int ret;
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2sin4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_IN4];
	struct mtk_afe_i2s_priv *i2sout4_priv = afe_priv->dai_priv[MT6989_DAI_I2S_OUT4];
	unsigned int ch_num_in;
	unsigned int ch_num_out;
	unsigned int sync_in;
	unsigned int sync_out;
	unsigned int ip_mode;

	/* get etdm ch */
	ret = of_property_read_u32(afe->dev->of_node, "etdm-out-ch", &ch_num_out);
	if (ret) {
		dev_info(afe->dev, "%s() failed to read etdm-out-ch\n", __func__);
		return -EINVAL;
	}
	i2sout4_priv->ch_num = ch_num_out;
	dev_info(afe->dev, "%s() etdm-out-ch: %d\n", __func__, ch_num_out);

	ret = of_property_read_u32(afe->dev->of_node, "etdm-in-ch", &ch_num_in);
	if (ret) {
		dev_info(afe->dev, "%s() failed to read etdm-in-ch\n", __func__);
		return -EINVAL;
	}
	i2sin4_priv->ch_num = ch_num_in;
	dev_info(afe->dev, "%s() etdm-in-ch: %d\n", __func__, ch_num_in);

	/* get etdm sync */
	ret = of_property_read_u32(afe->dev->of_node, "etdm-out-sync", &sync_out);
	if (ret) {
		dev_info(afe->dev, "%s() failed to read etdm-out-sync\n", __func__);
		return -EINVAL;
	}
	i2sout4_priv->sync = sync_out;
	dev_info(afe->dev, "%s() etdm-out-sync: %d\n", __func__, sync_out);

	ret = of_property_read_u32(afe->dev->of_node, "etdm-in-sync", &sync_in);
	if (ret) {
		dev_info(afe->dev, "%s() failed to read etdm-in-sync\n", __func__);
		return -EINVAL;
	}
	i2sin4_priv->sync = sync_in;
	dev_info(afe->dev, "%s() etdm-in-sync: %d\n", __func__, sync_in);

	/* get etdm ip mode */
	ret = of_property_read_u32(afe->dev->of_node, "etdm-ip-mode", &ip_mode);
	if (ret) {
		dev_info(afe->dev, "%s() failed to read etdm-ip-mode\n", __func__);
		return -EINVAL;
	}
	i2sin4_priv->ip_mode = ip_mode;
	dev_info(afe->dev, "%s() etdm-ip-mode: %d\n", __func__, ip_mode);


	return 0;
}

int mt6989_dai_i2s_get_share(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	const struct device_node *of_node = afe->dev->of_node;
	const char *of_str;
	const char *property_name;
	struct mtk_afe_i2s_priv *i2s_priv;
	int i;

	for (i = 0; i < DAI_I2S_NUM; i++) {
		i2s_priv = afe_priv->dai_priv[mt6989_i2s_priv[i].id];
		property_name = mt6989_i2s_priv[i].share_property_name;
		if (of_property_read_string(of_node, property_name, &of_str))
			continue;
		i2s_priv->share_i2s_id = get_i2s_id_by_name(afe, of_str);
	}

	return 0;
}

int mt6989_dai_i2s_set_priv(struct mtk_base_afe *afe)
{
	int i;
	int ret;

	for (i = 0; i < DAI_I2S_NUM; i++) {
		ret = mt6989_dai_set_priv(afe, mt6989_i2s_priv[i].id,
					  sizeof(struct mtk_afe_i2s_priv),
					  &mt6989_i2s_priv[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int mt6989_dai_i2s_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;
	int ret;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_i2s_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_i2s_driver);

	dai->controls = mtk_dai_i2s_controls;
	dai->num_controls = ARRAY_SIZE(mtk_dai_i2s_controls);
	dai->dapm_widgets = mtk_dai_i2s_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_i2s_widgets);
	dai->dapm_routes = mtk_dai_i2s_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_i2s_routes);

	/* set all dai i2s private data */
	ret = mt6989_dai_i2s_set_priv(afe);
	if (ret)
		return ret;

	/* parse share i2s */
	ret = mt6989_dai_i2s_get_share(afe);
	if (ret)
		return ret;

	/* for customer to change ch_num & sync & ipmode from dts */
	ret = etdm_parse_dt(afe);
	if (ret) {
		dev_info(afe->dev, "%s() fail to parse dts: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}
