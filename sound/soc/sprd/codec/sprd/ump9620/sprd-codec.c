/*
 * sound/soc/sprd/codec/sprd/ump9620/sprd-codec.c
 *
 * SPRD-CODEC -- SpreadTrum Tiger intergrated codec.
 *
 * Copyright (C) 2021 SpreadTrum Ltd.
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("ump9620")""fmt

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include "sprd-audio.h"
#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"
#include "sprd-codec.h"
#include "sprd-headset.h"

#define SOC_REG(r) ((unsigned short)(r))
#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#include "aud_topa_rf.h"

#define SPRD_CODEC_AP_BASE_HI (SPRD_CODEC_AP_BASE & 0xFFFF0000)
#define SPRD_CODEC_DP_BASE_HI (SPRD_CODEC_DP_BASE & 0xFFFF0000)

#define UNSUPPORTED_AD_RATE SNDRV_PCM_RATE_44100

#define SDM_RAMP_MAX 0x2000

/* Timeout (us) of polling the status */
#define DAC_CAL_DONE_TIMEOUT	(500 * 8)
#define DAC_CAL_POLL_DELAY_US	500

#define RCV_DVLD_DONE_TIMEOUT	(500 * 8)
#define RCV_DVLD_POLL_DELAY_US	500

enum PA_SHORT_T {
	PA_SHORT_NONE, /* PA output normal */
	PA_SHORT_VBAT, /* PA output P/N short VBAT */
	PA_SHORT_GND, /* PA output P/N short GND */
	PA_SHORT_CHECK_FAILED, /* error when do the check */
	PA_SHORT_MAX
};

enum CP_SHORT_T {
	CP_SHORT_NONE,
	FLYP_SHORT_POWER,
	FLYP_SHORT_GND,
	FLYN_SHORT_POWER,
	FLYN_SHORT_GND,
	VCPM_SHORT_POWER,
	VCPN_SHORT_GND,
	CP_SHORT_MAX
};

enum {
	SPRD_CODEC_DC_OS_SWITCH_ORDER = 3,
	SPRD_CODEC_DA_AO_PATH_ORDER = 11,
	SPRD_CODEC_ANA_MIXER_ORDER = 93,
	SPRD_CODEC_DEPOP_ORDER = 96,
	SPRD_CODEC_BUF_SWITCH_ORDER = 98,
	SPRD_CODEC_SDA_EN_ORDER = 101,
	SPRD_CODEC_DA_EN_ORDER = 102,
	SPRD_CODEC_SWITCH_ORDER = 103,
	SPRD_CODEC_DC_OS_ORDER = 104,
	SPRD_CODEC_DA_LDO_ORDER = 105,
	SPRD_CODEC_DA_BUF_DCCAL_ORDER = 107,
	SPRD_CODEC_AO_EN_ORDER = 108,
	SPRD_CODEC_RCV_EN_ORDER = 109,
	SPRD_CODEC_HP_EN_ORDER = 110,
	SPRD_CODEC_RCV_DEPOP_ORDER = 112,
	SPRD_CODEC_MIXER_ORDER = 115,/* Must be the last one */
};

enum {
	SPRD_CODEC_PLAYBACK,
	SPRD_CODEC_CAPTRUE,
	SPRD_CODEC_CAPTRUE1,
	SPRD_CODEC_CHAN_MAX,
};

enum {
	PSG_STATE_BOOST_NONE,
	PSG_STATE_BOOST_LARGE_GAIN,
	PSG_STATE_BOOST_SMALL_GAIN,
	PSG_STATE_BOOST_BYPASS,
};

static const char *sprd_codec_chan_name[SPRD_CODEC_CHAN_MAX] = {
	"DAC",
	"ADC",
	"ADC1",
};

enum IVSENCE_DMIC_TYPE {
	NONE,
	DMIC0,
	DMIC1,
	DMIC0_1,
	DMIC_TYPE_MAX
};

enum CLK_26M_IN_TYPE {
	AUD_26M_IN,
	AUD_PLL_IN,
	AUD_CLK_26M_IN_MAX
};

/*
 * only HW_CAL_MODE means do dac cali,
 * AA chip chose NOT_CAL or SW_CAL_MODE
 */
enum DA_BUF_CAL_MODE_TYPE {
	HW_CAL_MODE,
	SW_CAL_MODE,
	NOT_CAL,
	CAL_MODE_MAX
};

static const char * const da_buf_cal_mode_txt[CAL_MODE_MAX] = {
	[HW_CAL_MODE] = TO_STRING(HW_CAL_MODE),
	[SW_CAL_MODE] = TO_STRING(SW_CAL_MODE),
	[NOT_CAL] = TO_STRING(NOT_CAL),
};

enum HP_CHANNEL_TYPE {
	HP_LR_TYPE,
	HP_R_TYPE,
	HP_L_TYPE,
	HP_TYPE_MAX
};

static const char * const hp_chn_type_txt[HP_TYPE_MAX] = {
	[HP_LR_TYPE] = TO_STRING(HP_LR_TYPE),
	[HP_R_TYPE] = TO_STRING(HP_R_TYPE),
	[HP_L_TYPE] = TO_STRING(HP_L_TYPE),
};

enum INVERT_MODE_TYPE {
	NORMAL_MODE,
	INVERT_MODE,
	INVERT_MODE_MAX
};

static const char * const ivsence_dmic_type_txt[DMIC_TYPE_MAX] = {
	[NONE] = TO_STRING(NONE),
	[DMIC0] = TO_STRING(DMIC0),
	[DMIC1] = TO_STRING(DMIC1),
	[DMIC0_1] = TO_STRING(DMIC0_1),
};

static const struct soc_enum ivsence_dmic_sel_enum =
	SOC_ENUM_SINGLE_EXT(4, ivsence_dmic_type_txt);

static inline const char *sprd_codec_chan_get_name(int chan_id)
{
	return sprd_codec_chan_name[chan_id];
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int agdsp_access_enable(void)
	__attribute__ ((weak, alias("__agdsp_access_enable")));
static int __agdsp_access_enable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}

int agdsp_access_disable(void)
	__attribute__ ((weak, alias("__agdsp_access_disable")));
static int __agdsp_access_disable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}
#pragma GCC diagnostic pop

struct inter_pa {
	/* FIXME little endian */
	u32 DTRI_F_sel:3;
	u32 is_DEMI_mode:1;
	u32 is_classD_mode:1;
	u32 EMI_rate:3;
	u32 RESV:25;
};

struct pa_setting {
	union {
		struct inter_pa setting;
		u32 value;
	};
	int set;
};

static int sprd_pga_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
static int sprd_pga_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
static int sprd_mixer_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
static int sprd_mixer_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
static int sprd_codec_spk_pga_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);
static int sprd_codec_spk_pga_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);
static unsigned int sprd_codec_read(struct snd_soc_codec *codec,
				    unsigned int reg);

static unsigned long sprd_codec_dp_base;

enum {
	CODEC_PATH_DA = 0,
	CODEC_PATH_AD,
	CODEC_PATH_AD1,
	CODEC_PATH_MAX
};

enum {
	LRCLK_SEL_DAC,
	LRCLK_SEL_ADC,
	LRCLK_SEL_ADC1,
	LRCLK_SEL_MAX
};

struct fgu {
	unsigned int vh;
	unsigned int dvh;
	unsigned int vl;
	unsigned int dvl;
	unsigned int high_thrd;
	unsigned int low_thrd;
};

/* codec private data */
struct sprd_codec_priv {
	struct snd_soc_codec *codec;
	u32 da_sample_val;
	u32 ad_sample_val;
	u32 ad1_sample_val;
	int dp_irq;
	struct completion completion_dac_mute;

	struct pa_setting inter_pa;

	struct regulator *main_mic;
	struct regulator *head_mic;
	struct regulator *vb;

	int psg_state;
	struct fgu fgu;

	u32 fixed_sample_rate[CODEC_PATH_MAX];
	u32 lrclk_sel[LRCLK_SEL_MAX];
	u32 lrdat_sel;
	unsigned int replace_rate;
	enum PA_SHORT_T pa_short_stat;
	enum CP_SHORT_T cp_short_stat;

	u32 startup_cnt;
	struct mutex digital_enable_mutex;
	u32 digital_enable_count;
	u16 dac_switch;
	u16 adc_switch;
	u32 neg_cp_efuse;
	u32 fgu_4p2_efuse;
	u32 hp_mix_mode;
	u32 spk_dg;
	u32 spk_fall_dg;
	struct mutex dig_access_mutex;
	bool dig_access_en;
	bool user_dig_access_dis;
	enum IVSENCE_DMIC_TYPE ivsence_dmic_type;
	u8 drv_soft_t;
	u16 das_input_mux;
	bool daldo_bypass;
	enum CLK_26M_IN_TYPE clk_26m_in_sel;
	enum DA_BUF_CAL_MODE_TYPE daaol_buf_dccal;
	enum DA_BUF_CAL_MODE_TYPE daaor_buf_dccal;
	enum DA_BUF_CAL_MODE_TYPE dahpl_buf_dccal;
	enum DA_BUF_CAL_MODE_TYPE dahpr_buf_dccal;
	enum INVERT_MODE_TYPE dahp_os_inv;
	u16 dahp_os_d;
	enum HP_CHANNEL_TYPE hp_chn_type;
	/* from step 7, backup way to solve pop issue */
	bool rcv_pop_ramp;
	u32 sdm_l;
	u32 sdm_h;
	struct regmap *aon_top_apb;
	u32 clk26m_sinout_pmic_reg;
	u32 clk26m_sinout_pmic_mask;
	u32 ana_chip_id;
	u32 ana_chip_ver_id;
};

static const char * const das_input_mux_texts[] = {
	"L+R", "L*2", "R*2", "ZERO"
};

static const SOC_ENUM_SINGLE_DECL(
	das_input_mux_enum, SND_SOC_NOPM,
	0, das_input_mux_texts);

static const DECLARE_TLV_DB_SCALE(adc_tlv, 0, 300, 0);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -2400, 300, 1);
static const DECLARE_TLV_DB_SCALE(ear_tlv, -2400, 300, 1);
/* todo: remap it, ANA_CDC9[14:12] */
static const DECLARE_TLV_DB_SCALE(ao_tlv, 0, 150, 0);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -1500, 75, 0);

static const char * const lrclk_sel_text[] = {
	"normal", "invert"
};

static const struct soc_enum lrclk_sel_enum =
	SOC_ENUM_SINGLE_EXT(2, lrclk_sel_text);

static const char * const clk_26m_in_sel_text[] = {
	"in_26m", "in_pll"
};

static const struct soc_enum clk_26m_in_sel_enum =
	SOC_ENUM_SINGLE_EXT(2, clk_26m_in_sel_text);

static const struct soc_enum da_buf_dccal_mode_enum =
	SOC_ENUM_SINGLE_EXT(CAL_MODE_MAX, da_buf_cal_mode_txt);

static const struct soc_enum hp_chn_type_enum =
	SOC_ENUM_SINGLE_EXT(HP_TYPE_MAX, hp_chn_type_txt);

static const char * const codec_hw_info[] = {
	CODEC_HW_INFO
};

static const struct soc_enum codec_info_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(codec_hw_info), codec_hw_info);

#define SPRD_CODEC_PGA_M(xname, xreg, xshift, max, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, xreg, xshift, max, 0, \
		sprd_pga_get, sprd_pga_put, tlv_array)

#define SPRD_CODEC_PGA_MAX_INVERT(xname, xreg, xshift, max, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, xreg, xshift, max, 1, \
		sprd_pga_get, sprd_pga_put, tlv_array)

#define SPRD_CODEC_MIXER(xname, xreg, xshift)\
	SOC_SINGLE_EXT(xname, xreg, xshift, 1, 0, \
		sprd_mixer_get, sprd_mixer_put)

#define SPRD_CODEC_AD_MIXER(xname, xreg, xshift)\
	SOC_SINGLE_EXT(xname, xreg, xshift, 1, 0, \
		snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw)

static const struct snd_kcontrol_new ao_gain_controls[] = {
	SOC_SINGLE_EXT_TLV("AO Playback Volume", SOC_REG(ANA_CDC8), AO_G_S,
			   15, 0, sprd_codec_spk_pga_get,
			   sprd_codec_spk_pga_put, ao_tlv),
};

static const struct snd_kcontrol_new ear_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("EAR Playback Volume",
				  SOC_REG(ANA_CDC8), RCV_G_S, 15, ear_tlv),
		/* change  ANA_CDC7 to ANA_CDC8 */
};

static const struct snd_kcontrol_new hpl_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("HPL Playback Volume",
				  SOC_REG(ANA_CDC8), HPL_G_S, 15, hp_tlv),
};

static const struct snd_kcontrol_new hpr_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("HPR Playback Volume",
				  SOC_REG(ANA_CDC8), HPR_G_S, 15, hp_tlv),
};

static const struct snd_kcontrol_new adc_1_pga_controls[] = {
	SPRD_CODEC_PGA_M("ADC_1 Capture Volume",
			 SOC_REG(ANA_CDC2), ADPGA_1_G_S, 7, adc_tlv),
};

static const struct snd_kcontrol_new adc_2_pga_controls[] = {
	SPRD_CODEC_PGA_M("ADC_2 Capture Volume",
			 SOC_REG(ANA_CDC2), ADPGA_2_G_S, 7, adc_tlv),
};

static const struct snd_kcontrol_new adc_3_pga_controls[] = {
	SPRD_CODEC_PGA_M("ADC_3 Capture Volume",
			 SOC_REG(ANA_CDC2), ADPGA_3_G_S, 7, adc_tlv),
};

static const struct snd_kcontrol_new dac_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("DAC Playback Volume",
				  SOC_REG(ANA_DAC0), DA_IG_S, 2, dac_tlv),
};

/* ADC_1 Mixer */
static const struct snd_kcontrol_new adc_1_mixer_controls[] = {
	SPRD_CODEC_AD_MIXER("MIC1PGA_1 Switch", SOC_REG(ANA_CDC1),
			    SMIC1PGA_1_S),
	SPRD_CODEC_AD_MIXER("HPMICPGA_1 Switch", SOC_REG(ANA_CDC1),
			    SHMICPGA_1_S),
};

/* ADC_2 Mixer */
static const struct snd_kcontrol_new adc_2_mixer_controls[] = {
	SPRD_CODEC_AD_MIXER("MIC2PGA_2 Switch", SOC_REG(ANA_CDC1),
			    SMIC2PGA_2_S),
	SPRD_CODEC_AD_MIXER("HPMICPGA_2 Switch", SOC_REG(ANA_CDC1),
			    SHMICPGA_2_S),
};

/* ADC_3 Mixer */
static const struct snd_kcontrol_new adc_3_mixer_controls[] = {
	SPRD_CODEC_AD_MIXER("MIC3PGA_3 Switch", SOC_REG(ANA_CDC1),
			    SMIC3PGA_3_S),
	SPRD_CODEC_AD_MIXER("HPMICPGA_3 Switch", SOC_REG(ANA_CDC1),
			    SHMICPGA_3_S),
};

/* HPL Mixer */
static const struct snd_kcontrol_new hpl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPL Switch", SND_SOC_NOPM, SDALHPL_HPL_S),
	SPRD_CODEC_MIXER("DACRHPL Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCLHPL Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCRHPL Switch", SND_SOC_NOPM, 0),
};

/* HPR Mixer */
static const struct snd_kcontrol_new hpr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPR Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("DACRHPR Switch", SND_SOC_NOPM, SDARHPR_HPR_S),
	SPRD_CODEC_MIXER("ADCLHPR Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCRHPR Switch", SND_SOC_NOPM, 0),
};

/* AO Mixer */
static const struct snd_kcontrol_new dacao_mixer_controls[] = {
	SPRD_CODEC_MIXER("AOL Switch", SND_SOC_NOPM, SDAAOL_AO_S),
	SPRD_CODEC_MIXER("AOR Switch", SND_SOC_NOPM, SDAAOR_AO_S),
	SPRD_CODEC_MIXER("ADCLAOL Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCRAOL Switch", SND_SOC_NOPM, 0),
};

static const struct snd_kcontrol_new ear_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACHPL Switch", SND_SOC_NOPM, SDAHPL_RCV_S),
	SPRD_CODEC_MIXER("DACAOL Switch", SND_SOC_NOPM, SDAAOL_RCV_S),
};

static const struct snd_kcontrol_new loop_controls[] = {
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		       1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		       1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		       1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		       1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
};

static const struct snd_kcontrol_new da_mode_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new virt_output_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new aud_adc_switch[] = {
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
};

static const struct snd_kcontrol_new dac_ao_hp_switch[] = {
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
};

static int sprd_codec_clk26m_sinout(struct sprd_codec_priv *sprd_codec,
				    bool clk_on)
{
	unsigned int regval = 0;
	int ret;

	regmap_read(sprd_codec->aon_top_apb, sprd_codec->clk26m_sinout_pmic_reg,
		    &regval);
	sp_asoc_pr_dbg("%s clk26m_sinout_pmic, ret %d, access value 0x%x\n",
		       __func__, ret, regval);
	ret = regmap_update_bits(sprd_codec->aon_top_apb,
		sprd_codec->clk26m_sinout_pmic_reg,
		sprd_codec->clk26m_sinout_pmic_mask,
		clk_on ? sprd_codec->clk26m_sinout_pmic_mask : 0);
	regmap_read(sprd_codec->aon_top_apb, sprd_codec->clk26m_sinout_pmic_reg,
		    &regval);
	sp_asoc_pr_dbg("%s update clk26m_sinout_pmic, ret %d, access value 0x%x\n",
		       __func__, ret, regval);

	return ret;
}

static const u16 ocp_pfm_cfg_table[16] = {
	0x0406, 0x1618, 0x292b, 0x3a3b, 0x4344, 0x5457, 0x6668, 0x787a,
	0x8082, 0x9095, 0xa4a7, 0xb6b8, 0xc0c0, 0xd0d3, 0xe3e5, 0xf5f6
};

static int sprd_codec_power_get(struct device *dev, struct regulator **regu,
				const char *id)
{
	if (!*regu) {
		*regu = regulator_get(dev, id);
		if (IS_ERR(*regu)) {
			pr_err("ERR:Failed to request %ld: %s\n",
			       PTR_ERR(*regu), id);
			*regu = 0;
			return -1;
		}
	}
	return 0;
}

static int sprd_codec_power_put(struct regulator **regu)
{
	if (*regu) {
		regulator_set_mode(*regu, REGULATOR_MODE_NORMAL);
		regulator_disable(*regu);
		regulator_put(*regu);
		*regu = 0;
	}
	return 0;
}

static void codec_digital_reg_restore(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&sprd_codec->digital_enable_mutex);
	if (sprd_codec->digital_enable_count)
		arch_audio_codec_digital_reg_enable_v2();
	mutex_unlock(&sprd_codec->digital_enable_mutex);
}

static int dig_access_disable_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	bool disable = !!ucontrol->value.integer.value[0];

	mutex_lock(&sprd_codec->dig_access_mutex);
	if (sprd_codec->dig_access_en) {
		if (disable == sprd_codec->user_dig_access_dis) {
			mutex_unlock(&sprd_codec->dig_access_mutex);
			return 0;
		}
		if (disable) {
			sp_asoc_pr_dbg("%s, disable agdsp access\n", __func__);
			sprd_codec->user_dig_access_dis = disable;
			agdsp_access_disable();
		} else {
			sp_asoc_pr_dbg("%s, enable agdsp access\n", __func__);
			if (agdsp_access_enable()) {
				pr_err("%s, agdsp_access_enable failed!\n",
				       __func__);
				mutex_unlock(&sprd_codec->dig_access_mutex);
				return -EIO;
			}
			codec_digital_reg_restore(codec);
			sprd_codec->user_dig_access_dis = disable;
		}
	}
	mutex_unlock(&sprd_codec->dig_access_mutex);

	return 0;
}

static int dig_access_disable_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->user_dig_access_dis;

	return 0;
}

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
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		return 0;
	}

	return ev_name;
}

/* unit: ms */
static void sprd_codec_wait(u32 wait_time)
{
	pr_debug("%s %d ms\n", __func__, wait_time);

	if (wait_time < 0)
		return;
	if (wait_time < 20)
		usleep_range(wait_time * 1000, wait_time * 1000 + 200);
	else
		msleep(wait_time);
}

static int sprd_codec_read_efuse(struct platform_device *pdev,
				 const char *cell_name, u32 *data)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len;

	cell = nvmem_cell_get(&pdev->dev, cell_name);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	*data = calib_data;
	kfree(buf);

	return 0;
}

static int sprd_pga_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int mask = BIT(fls(mc->max)) - 1;
	unsigned int val;

	val = snd_soc_read(codec, mc->reg);
	val = (val >> mc->shift) & mask;

	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int sprd_pga_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int mask = BIT(fls(mc->max)) - 1;
	unsigned int val = ucontrol->value.integer.value[0];

	snd_soc_update_bits(codec, mc->reg,
			    mask << mc->shift,
			    val << mc->shift);

	return 0;
}

static int sprd_codec_spk_pga_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->spk_dg;
	return 0;
}

static int sprd_codec_spk_pga_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->spk_dg = ucontrol->value.integer.value[0];
	if (sprd_codec->psg_state > PSG_STATE_BOOST_LARGE_GAIN)
		ucontrol->value.integer.value[0] = sprd_codec->spk_fall_dg;
	sprd_pga_put(kcontrol, ucontrol);
	return 0;
}

static int sprd_mixer_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	val = sprd_codec->dac_switch & (1 << mc->shift);
	ucontrol->value.integer.value[0] = val;

	sp_asoc_pr_info("dac switch 0x%x,shift %d, get %d\n",
			sprd_codec->dac_switch, mc->shift,
			(int)ucontrol->value.integer.value[0]);

	return 0;
}

static int sprd_mixer_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	val = 1 << mc->shift;
	if (ucontrol->value.integer.value[0])
		sprd_codec->dac_switch |= val;
	else
		sprd_codec->dac_switch &= ~val;

	sp_asoc_pr_info("dac switch 0x%x,shift %d,set %d\n",
			sprd_codec->dac_switch, mc->shift,
			(int)ucontrol->value.integer.value[0]);

	snd_soc_dapm_put_volsw(kcontrol, ucontrol);
	return 0;
}

static int sprd_codec_reg_read_poll_timeout(struct snd_soc_codec *codec,
					    unsigned int addr,
					    unsigned int check1,
					    unsigned int check2,
					    unsigned int sleep_us,
					    unsigned int timeout_us)
{
	ktime_t timeout = ktime_add_us(ktime_get(), timeout_us);
	unsigned int cond;
	int val;

	might_sleep_if(sleep_us);
	for (;;) {
		val = sprd_codec_read(codec, addr);
		if (val < 0)
			break;

		cond = ((val & check1) && (val & check2));
		if (cond)
			break;

		if (timeout_us && ktime_compare(ktime_get(), timeout) > 0) {
			val = sprd_codec_read(codec, addr);
			if (val >= 0)
				cond = ((val & check1) && (val & check2));
			sp_asoc_pr_info("%s val %d 0x%x, check1 0x%x, check2 0x%x, cond 0x%x\n",
					__func__, val, val, check1, check2,
					cond);
			break;
		}
		if (sleep_us)
			usleep_range((sleep_us >> 2) + 1, sleep_us);
	}

	return (val < 0) ? val : (cond ? 0 : -ETIMEDOUT);
}

static void update_switch(struct snd_soc_codec *codec, u32 path, u32 on)
{
	u32 val = 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	if (on)
		val = sprd_codec->dac_switch & path;
	sp_asoc_pr_info("%s path 0x%x, on %d, dac_switch 0x%x, val 0x%x\n",
			__func__, path, on, sprd_codec->dac_switch, val);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC9), path, val);
}

static inline void sprd_codec_vcm_v_sel(struct snd_soc_codec *codec, int v_sel)
{
/* yintang marked for compiling.
 * int mask;
 * int val;
 * sp_asoc_pr_dbg("VCM Set %d\n", v_sel);
 * mask = VB_V_MASK << VB_V;
 * val = (v_sel << VB_V) & mask;
 * snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), mask, val);
 */
}

static void sprd_codec_sdm_init(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u32 val, mask;

	if (sprd_codec->sdm_l)
		val = sprd_codec->sdm_l;
	else
		val = 0x9999;

	if (sprd_codec->sdm_h)
		mask = sprd_codec->sdm_h;
	else
		mask = 1;

	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L), 0xffff, val);
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_H), 0xffff, mask);
	sp_asoc_pr_info("ramp reg: SDM_L 0x%x, SDM_H 0x%x\n", val, mask);
}

static int daao_os_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_audif_dc_os_set(codec, 6);
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC2), DAAO_OS_EN,
				    DAAO_OS_EN);
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC2), DAAO_OS_INV, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC2), DAAO_OS_D(0x7),
				    DAAO_OS_D(0x7));
		break;
	case SND_SOC_DAPM_POST_PMU:
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC2), DAAO_OS_EN, 0);
		break;
	default:
		break;
	}

	return 0;
}

static inline void sprd_codec_inter_pa_init(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec->inter_pa.setting.DTRI_F_sel = 0x2;
	sprd_codec->spk_fall_dg = 0x1;
}

static void sprd_hp_thd_n(struct snd_soc_codec *codec)
{
	int val, mask;
	val = DALDO_V(0x3) | DALDO_TRIM(0xf);
	mask = DALDO_V(0x3) | DALDO_TRIM(0xf);

	snd_soc_update_bits(codec, SOC_REG(ANA_DAC1), DAHP_BUF_ITRIM,
			DAHP_BUF_ITRIM);
	snd_soc_update_bits(codec, SOC_REG(ANA_DAC0), mask, val);
}

static int codec_widget_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	sp_asoc_pr_dbg("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		break;
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		break;
	}

	return 0;
}

static int sdaaol_ao_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	update_switch(codec, SDAAOL_AO, on);

	return 0;
}

static int sdaaor_ao_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	update_switch(codec, SDAAOR_AO, on);

	return 0;
}

static int sdaaol_rcv_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	update_switch(codec, SDAAOL_RCV, on);

	return 0;
}

static int sdahpl_rcv_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	update_switch(codec, SDAHPL_RCV, on);

	return 0;
}

static int sdm_dc_os_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	if (on) {
		sprd_codec_sdm_init(codec);
		sprd_codec_audif_dc_os_set(codec, 6);
	}

	return 0;
}

static int dahp_os_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u16 val;
	int ret = 0;

	sp_asoc_pr_info("'%s' %s, dahp_os_inv %d, dahp_os_d %d\n",
			w->name, get_event_name(event), sprd_codec->dahp_os_inv,
			sprd_codec->dahp_os_d);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC1), DAHP_OS_EN,
				    DAHP_OS_EN);
		sprd_hp_thd_n(codec);
		if (sprd_codec->dahp_os_inv == INVERT_MODE)
			snd_soc_update_bits(codec, SOC_REG(ANA_DAC1),
					    DAHP_OS_INV, DAHP_OS_INV);

		if (sprd_codec->dahp_os_d)
			val = sprd_codec->dahp_os_d;
		else
			val = 0x7;
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC1), DAHP_OS_D(0x7),
				    DAHP_OS_D(val));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC1), DAHP_OS_INV, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC1), DAHP_OS_D(0x7),
				    0);
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC1), DAHP_OS_EN, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int drv_soft_t_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s, drv_soft_t %d\n", w->name,
			get_event_name(event), sprd_codec->drv_soft_t);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* needn't to update regs when drv_soft_t is 0 */
		if (sprd_codec->drv_soft_t)
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC12),
					    DRV_SOFT_T(0x7),
					    DRV_SOFT_T(sprd_codec->drv_soft_t));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* check further */
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static enum DA_BUF_CAL_MODE_TYPE
get_dccal_mode(struct sprd_codec_priv *sprd_codec,
	       enum DA_BUF_CAL_MODE_TYPE mode)
{
	enum DA_BUF_CAL_MODE_TYPE dccal_mode;

	if (sprd_codec->ana_chip_ver_id == AUDIO_9620_VER_AA)
		dccal_mode = NOT_CAL;
	else
		dccal_mode = mode;

	sp_asoc_pr_info("dccal select %s %s %s\n",
			da_buf_cal_mode_txt[dccal_mode],
			(mode == dccal_mode) ? "" : "from",
			(mode == dccal_mode) ? "" : da_buf_cal_mode_txt[mode]);
	return dccal_mode;
}

static int daaol_buf_cal_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	enum DA_BUF_CAL_MODE_TYPE dccal_mode;
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dccal_mode = get_dccal_mode(sprd_codec,
					    sprd_codec->daaol_buf_dccal);
		switch (dccal_mode) {
		case HW_CAL_MODE:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOL_BUF_CAL_MODE_SEL, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOL_BUF_DCCAL_EN,
					    DAAOL_BUF_DCCAL_EN);
			ret = sprd_codec_reg_read_poll_timeout(codec,
				SOC_REG(ANA_CDC7),
				DAAOL_VRPBUF_CAL_DONE,
				DAAOL_VRNBUF_CAL_DONE,
				DAC_CAL_POLL_DELAY_US,
				DAC_CAL_DONE_TIMEOUT);
			if (ret)
				pr_err("%s check cal done fail\n",
				       __func__, ret);
			break;
		case SW_CAL_MODE:
		case NOT_CAL:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOL_BUF_DCCAL_EN, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOL_BUF_CAL_MODE_SEL,
					    DAAOL_BUF_CAL_MODE_SEL);
			break;
		default:
			ret = -EINVAL;
			pr_err("daaol_buf_dccal error %d\n",
			       sprd_codec->daaol_buf_dccal);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAAOL_BUF_DCCAL_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAAOL_BUF_CAL_MODE_SEL, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int daaor_buf_cal_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	enum DA_BUF_CAL_MODE_TYPE dccal_mode;
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dccal_mode = get_dccal_mode(sprd_codec,
					    sprd_codec->daaor_buf_dccal);
		switch (dccal_mode) {
		case HW_CAL_MODE:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOR_BUF_CAL_MODE_SEL, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOR_BUF_DCCAL_EN,
					    DAAOR_BUF_DCCAL_EN);
			ret = sprd_codec_reg_read_poll_timeout(codec,
					SOC_REG(ANA_CDC7),
					DAAOR_VRPBUF_CAL_DONE,
					DAAOR_VRNBUF_CAL_DONE,
					DAC_CAL_POLL_DELAY_US,
					DAC_CAL_DONE_TIMEOUT);
			if (ret)
				pr_err("%s check cal done fail %d\n",
				       __func__, ret);
			break;
		case SW_CAL_MODE:
		case NOT_CAL:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOR_BUF_DCCAL_EN, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAAOR_BUF_CAL_MODE_SEL,
					    DAAOR_BUF_CAL_MODE_SEL);
			break;
		default:
			ret = -EINVAL;
			pr_err("daaor_buf_dccal %d error\n",
			       sprd_codec->daaor_buf_dccal);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAAOR_BUF_DCCAL_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAAOR_BUF_CAL_MODE_SEL, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int dahpl_buf_cal_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	enum DA_BUF_CAL_MODE_TYPE dccal_mode;
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dccal_mode = get_dccal_mode(sprd_codec,
					    sprd_codec->dahpl_buf_dccal);
		switch (dccal_mode) {
		case HW_CAL_MODE:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPL_BUF_CAL_MODE_SEL, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPL_BUF_DCCAL_EN,
					    DAHPL_BUF_DCCAL_EN);
			/* check further */
			ret = sprd_codec_reg_read_poll_timeout(codec,
					SOC_REG(ANA_CDC7),
					DAHPL_VRPBUF_CAL_DONE,
					DAHPL_VRNBUF_CAL_DONE,
					DAC_CAL_POLL_DELAY_US,
					DAC_CAL_DONE_TIMEOUT);
			if (ret)
				pr_err("%s check cal done fail, val 0x%x, %d\n",
				       __func__, ret);
			break;
		case SW_CAL_MODE:
		case NOT_CAL:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPL_BUF_DCCAL_EN, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPL_BUF_CAL_MODE_SEL,
					    DAHPL_BUF_CAL_MODE_SEL);
			break;
		default:
			ret = -EINVAL;
			pr_err("%s cal mode %d error\n", __func__,
			       sprd_codec->dahpl_buf_dccal);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAHPL_BUF_DCCAL_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAHPL_BUF_CAL_MODE_SEL, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int dahpr_buf_cal_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	enum DA_BUF_CAL_MODE_TYPE dccal_mode;
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dccal_mode = get_dccal_mode(sprd_codec,
					    sprd_codec->dahpr_buf_dccal);
		switch (dccal_mode) {
		case HW_CAL_MODE:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPR_BUF_CAL_MODE_SEL, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPR_BUF_DCCAL_EN,
					    DAHPR_BUF_DCCAL_EN);
			ret = sprd_codec_reg_read_poll_timeout(codec,
					SOC_REG(ANA_CDC7),
					DAHPR_VRPBUF_CAL_DONE,
					DAHPR_VRNBUF_CAL_DONE,
					DAC_CAL_POLL_DELAY_US,
					DAC_CAL_DONE_TIMEOUT);
			if (ret)
				pr_err("%s check cal done fail, val 0x%x, %d\n",
				       __func__, ret);
			break;
		case SW_CAL_MODE:
		case NOT_CAL:
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPR_BUF_DCCAL_EN, 0);
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
					    DAHPR_BUF_CAL_MODE_SEL,
					    DAHPR_BUF_CAL_MODE_SEL);
			break;
		default:
			ret = -EINVAL;
			pr_err("%s cal mode %d error\n", __func__,
			       sprd_codec->dahpr_buf_dccal);
		}
		usleep_range(20, 30);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAHPR_BUF_DCCAL_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAHPR_BUF_CAL_MODE_SEL, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int sprd_codec_set_sample_rate(struct snd_soc_codec *codec,
				      u32 rate,
				      int mask, int shift)
{
	int i = 0;
	unsigned int val = 0;
	int ret;
	struct sprd_codec_rate_tbl {
		u32 rate; /* 8000, 11025, ... */
		/* SPRD_CODEC_RATE_xxx, ... */
		u32 sprd_rate;
	} rate_tbl[] = {
		/* rate in Hz, SPRD rate format */
		{48000, SPRD_CODEC_RATE_48000},
		{8000, SPRD_CODEC_RATE_8000},
		{11025, SPRD_CODEC_RATE_11025},
		{16000, SPRD_CODEC_RATE_16000},
		{22050, SPRD_CODEC_RATE_22050},
		{32000, SPRD_CODEC_RATE_32000},
		{44100, SPRD_CODEC_RATE_44100},
		{96000, SPRD_CODEC_RATE_96000},
	};

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s agdsp_access_enable error %d", __func__, ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(rate_tbl); i++) {
		if (rate_tbl[i].rate == rate)
			val = rate_tbl[i].sprd_rate;
	}

	if (val)
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    val << shift);
	else
		pr_err("error, SPRD-CODEC not support rate %d\n", rate);

	sp_asoc_pr_dbg("Set Playback rate 0x%x\n",
		       snd_soc_read(codec, AUD_DAC_CTL));

	agdsp_access_disable();

	return 0;
}

static int is_unsupported_ad_rate(unsigned int rate)
{
	return (snd_pcm_rate_to_rate_bit(rate) & UNSUPPORTED_AD_RATE);
}

static int sprd_codec_set_ad_sample_rate(struct snd_soc_codec *codec,
					 u32 rate,
					 int mask, int shift)
{
	int set;
	int ret;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int replace_rate = sprd_codec->replace_rate;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s agdsp_access_enable error %d", __func__, ret);
		return ret;
	}

	if (is_unsupported_ad_rate(rate) && replace_rate) {
		pr_debug("Replace %uHz with %uHz for record\n",
			 rate, replace_rate);
		rate = replace_rate;
	}

	set = rate / 4000;
	if (set > 13)
		pr_err("error, SPRD-CODEC not support ad rate %d\n", rate);
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC_CTL), mask << shift,
			    set << shift);

	agdsp_access_disable();

	return 0;
}

static int sprd_codec_sample_rate_setting(struct sprd_codec_priv *sprd_codec)
{
	int ret;

	sp_asoc_pr_info("%s AD %u DA %u AD1 %u\n", __func__,
			sprd_codec->ad_sample_val, sprd_codec->da_sample_val,
			sprd_codec->ad1_sample_val);
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s agdsp_access_enable error %d", __func__, ret);
		return ret;
	}

	if (sprd_codec->ad_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
					      sprd_codec->ad_sample_val,
					      ADC_SRC_N_MASK, ADC_SRC_N);
	}
	if (sprd_codec->ad1_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
					      sprd_codec->ad1_sample_val,
					      ADC1_SRC_N_MASK, ADC1_SRC_N);
	}
	if (sprd_codec->da_sample_val) {
		sprd_codec_set_sample_rate(sprd_codec->codec,
					   sprd_codec->da_sample_val,
					   DAC_FS_MODE_MASK, DAC_FS_MODE);
	}

	agdsp_access_disable();

	return 0;
}

static void sprd_codec_power_disable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ADEBUG();

	arch_audio_codec_analog_disable();
	sprd_codec_audif_clk_enable(codec, 0);
	regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_STANDBY);
}

static void sprd_codec_power_enable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ADEBUG();

	arch_audio_codec_analog_enable();
	sprd_codec_audif_clk_enable(codec, 1);
	regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_NORMAL);
}

static int sprd_codec_digital_open(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	ADEBUG();

	/* FIXME : Some Clock SYNC bug will cause MUTE */
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), BIT(DAC_MUTE_EN), 0);

	sprd_codec_sample_rate_setting(sprd_codec);

	/*peng.lee added this according to janus.li's email*/
	snd_soc_update_bits(codec, SOC_REG(AUD_SDM_CTL0), 0xFFFF, 0);

	/* Set the left/right clock selection. */
	if (sprd_codec->lrclk_sel[LRCLK_SEL_DAC])
		snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL),
				    BIT(DAC_LR_SEL), BIT(DAC_LR_SEL));
	if (sprd_codec->lrclk_sel[LRCLK_SEL_ADC])
		snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL),
				    BIT(ADC_LR_SEL), BIT(ADC_LR_SEL));
	if (sprd_codec->lrclk_sel[LRCLK_SEL_ADC1])
		snd_soc_update_bits(codec, SOC_REG(AUD_ADC1_I2S_CTL),
				    BIT(ADC1_LR_SEL), BIT(ADC1_LR_SEL));

	/*
	 * temporay method to disable DNS, waiting ASIC to improve
	 * this feature
	 */

	snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN), 0xffff,
			    0x3303);
	snd_soc_update_bits(codec, SOC_REG(AUD_DNS_SW), BIT(RG_DNS_SW), 0);


	return ret;
}

static void codec_digital_reg_enable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&sprd_codec->digital_enable_mutex);
	if (!sprd_codec->digital_enable_count ||
	    sprd_codec->user_dig_access_dis)
		arch_audio_codec_digital_reg_enable_v2();
	sprd_codec->digital_enable_count++;
	mutex_unlock(&sprd_codec->digital_enable_mutex);
}

static void codec_digital_reg_disable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&sprd_codec->digital_enable_mutex);

	if (sprd_codec->digital_enable_count)
		sprd_codec->digital_enable_count--;

	if (!sprd_codec->digital_enable_count)
		arch_audio_codec_digital_reg_disable_v2();

	mutex_unlock(&sprd_codec->digital_enable_mutex);
}

static int digital_power_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mutex_lock(&sprd_codec->dig_access_mutex);
		ret = agdsp_access_enable();
		if (ret) {
			pr_err("%s agdsp_access_enable failed\n", __func__);
			mutex_unlock(&sprd_codec->dig_access_mutex);
			return ret;
		}
		sprd_codec->dig_access_en = true;
		mutex_unlock(&sprd_codec->dig_access_mutex);

		codec_digital_reg_enable(codec);
		sprd_codec_digital_open(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * maybe ADC module use it, so we cann't close it
		 * arch_audio_codec_digital_disable();
		 */
		codec_digital_reg_disable(codec);

		mutex_lock(&sprd_codec->dig_access_mutex);
		if (sprd_codec->dig_access_en) {
			sprd_codec->user_dig_access_dis = false;
			agdsp_access_disable();
			sprd_codec->dig_access_en = false;
		}
		mutex_unlock(&sprd_codec->dig_access_mutex);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int clk_26m_in_sel_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_info("'%s' %s, clk_26m_in_sel %d\n", w->name,
			get_event_name(event), sprd_codec->clk_26m_in_sel);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (sprd_codec->clk_26m_in_sel == AUD_PLL_IN)
			snd_soc_update_bits(codec, SOC_REG(ANA_CLK0),
					    CLK_26M_IN_SEL, CLK_26M_IN_SEL);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CLK0), CLK_26M_IN_SEL,
				    0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int analog_power_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_dbg("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = sprd_codec_clk26m_sinout(sprd_codec, true);
		if (ret)
			pr_err("%s set clk26m_sinout_pmic fail\n");
		sprd_codec_power_enable(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_power_disable(codec);
		ret = sprd_codec_clk26m_sinout(sprd_codec, false);
		if (ret)
			pr_err("%s clean clk26m_sinout_pmic fail\n");
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int audio_dcl_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	unsigned int val;

	/*
	 * check further, negative effects?
	 * RSTN_AUD_DCL_32K RSTN_AUD_DIG_DRV_SOFT for hp, rcv, lineout
	 * RSTN_AUD_HPDPOP for hp, RSTN_AUD_DIG_DCL for hp, rcv
	 * RSTN_AUD_DIG_INTC for hp, rcv
	 */
	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		val = RSTN_AUD_DCL_32K | RSTN_AUD_DIG_DRV_SOFT |
			RSTN_AUD_HPDPOP | RSTN_AUD_DIG_DCL;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL0), val, val);

		val = RSTN_AUD_DIG_INTC;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL1), val, val);
		usleep_range(180, 200);

		val = DAHP_DSEL_R2L | DAHP_DSEL_L2R;
		if (sprd_codec->lrdat_sel)
			snd_soc_update_bits(codec, SOC_REG(ANA_DAC3), val, val);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int daldo_en_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u32 val;
	u32 mask;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	if (sprd_codec->daldo_bypass)
		val = DALDO_BYPASS;
	else
		val = DALDO_EN;

	mask = DALDO_EN | DALDO_BYPASS;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC0), mask, val);
		usleep_range(600, 610);
		break;
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_DAC0), mask, 0);
		break;
	default:
		break;
	}

	return 0;
}

static int chan_event(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol, int event)
{
	int chan_id = FUN_REG(w->reg);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("'%s' '%s', %s %s\n", w->name, w->sname,
			sprd_codec_chan_get_name(chan_id),
			STR_ON_OFF(on));

	return 0;
}

static int dfm_out_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_info("DFM-OUT %s\n", STR_ON_OFF(on));
	sprd_codec_sample_rate_setting(sprd_codec);

	return 0;
}

static void sprd_codec_init_fgu(struct sprd_codec_priv *sprd_codec)
{
	struct fgu *fgu = &sprd_codec->fgu;

	fgu->vh = 3300;
	fgu->vl = 3250;
	fgu->dvh = 50;
	fgu->dvl = 50;
}

void sprd_codec_intc_irq(struct snd_soc_codec *codec, u32 int_shadow)
{
	u32 val_s, val_n, val_fgu;

	val_n = PA_DCCAL_INT_SHADOW_STATUS |
		PA_CLK_CAL_INT_SHADOW_STATUS |
		RCV_DPOP_INT_SHADOW_STATUS |
		HPR_DPOP_INT_SHADOW_STATUS |
		HPL_DPOP_INT_SHADOW_STATUS |
		IMPD_DISCHARGE_INT_SHADOW_STATUS |
		IMPD_CHARGE_INT_SHADOW_STATUS |
		IMPD_BIST_INT_SHADOW_STATUS;
	val_s = HPR_SHUTDOWN_INT_SHADOW_STATUS |
		HPL_SHUTDOWN_INT_SHADOW_STATUS |
		AO_SHUTDOWN_INT_SHADOW_STATUS |
		EAR_SHUTDOWN_INT_SHADOW_STATUS;
	val_fgu = FGU_LOW_LIMIT_INT_SHADOW_STATUS;

	/* For such int, nothing need to be done, just keep current status */
	if (int_shadow & val_s) {
		sp_asoc_pr_info("IRQ shut down int_shadow 0x%x\n",
				int_shadow);
		return;
	}

	/* For such int, clear and enable it again.*/
	if (int_shadow & val_n)
		sp_asoc_pr_info("IRQ clear and enable, int_shadow 0x%x\n",
				int_shadow);

	/* Int cannot be cleared within about 200ms, so here we should
	 * wait 200ms to avoid Int trigger continuesely.
	 */
	sprd_codec_wait(250);
}

static int fdin_dvld_check_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	u32 state;
	int i = 0;

	while (i++ < 10) {
		sprd_codec_wait(10);
		state = snd_soc_read(codec, SOC_REG(ANA_STS1)) &
			RCV_DAC_FDIN_DVLD;
		if ((on && state) || (!on && !state))
			break;
	}

	/* check further, do something in PMD? */
	if (sprd_codec->rcv_pop_ramp && on) {
		for (i = 4; i >= 0; i--) {
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC8),
					    RCV_G(0xf), RCV_G(i));
			usleep_range(100, 110);
		}
	}

	sp_asoc_pr_info("'%s' %s, check %s i %d\n", w->name,
			get_event_name(event),
			(i >= 10) ? "failed" : "sucessful", i);

	return 0;
}

static int ear_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC13), RCV_CUR, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC10), RCV_EN, RCV_EN);
		ret = sprd_codec_reg_read_poll_timeout(codec, SOC_REG(ANA_STS1),
						       RCV_DCCAL_DVLD,
						       RCV_LOOP_DVLD,
						       RCV_DVLD_POLL_DELAY_US,
						       RCV_DVLD_DONE_TIMEOUT);
		if (ret)
			pr_err("%s check rcv dvld fail, val 0x%x, %d\n",
			       __func__, ret);

		if (sprd_codec->rcv_pop_ramp)
			snd_soc_update_bits(codec, SOC_REG(ANA_CDC8),
					    RCV_G(0xf),
					    RCV_G(0x5));
		/* check further, need to do something in PMD? */
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC10), RCV_EN, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int hp_depop_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u32 val, mask;
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	if (on) {
		val = HP_DPOP_EN | HP_DPOP_AUTO_EN;
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_DCL5), val, val);

		val = HP_DPOP_CHG_OFF_PD | HPL_DPOP_CHG | HPR_DPOP_CHG;
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_DCL5), val, 0);

		snd_soc_update_bits(codec, SOC_REG(ANA_DCL7), HP_DCV_VAL1(0xf),
				    HP_DCV_VAL1(0x1));
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL8), HP_DCV_VAL2(0xf),
				    HP_DCV_VAL2(0x6));

		mask = HP_DPOP_FDIN_EN | HP_DPOP_FDOUT_EN;
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_DCL5), mask, mask);
	}

	return ret;
}

static int adc_1_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_1_CLK_RST,
				    ADC_1_CLK_RST);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_1_CLK_RST, 0);
		/*
		 * ADC_x_RST should keep be 0-1-0,
		 * and 1 state should keep 20 cycle of 6.5M clock at least.
		 */
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_1_RST, 0);
		udelay(4);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_1_RST,
				    ADC_1_RST);
		udelay(4);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_1_RST, 0);
		/*
		 * wait 60ms is by experience for the POP,
		 * the duration of POP is about 70ms.
		 */
		sprd_codec_wait(60);/* check further */
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int adc_2_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0),
				    ADC_2_CLK_RST, ADC_2_CLK_RST);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_2_CLK_RST, 0);

		/*
		 * ADC_x_RST should keep be 0-1-0,
		 * and 1 state should keep 20 cycle of 6.5M clock at least.
		 */
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_2_RST, 0);
		udelay(4);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_2_RST,
				    ADC_2_RST);
		udelay(4);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_2_RST, 0);
		/*
		 * wait 60ms is by experience for the POP,
		 * the duration of POP is about 70ms.
		 */
		sprd_codec_wait(60);/* check further */
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int adc_3_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC1),
				    ADC_3_CLK_RST, ADC_3_CLK_RST);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC1), ADC_3_CLK_RST, 0);

		/*
		 * ADC_x_RST should keep be 0-1-0,
		 * and 1 state should keep 20 cycle of 6.5M clock at least.
		 */
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_3_RST, 0);
		udelay(4);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_3_RST,
				    ADC_3_RST);
		udelay(4);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADC_3_RST, 0);
		/*
		 * wait 60ms is by experience for the POP,
		 * the duration of POP is about 70ms.
		 */
		sprd_codec_wait(60);/* check further */
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int sprd_ivsense_src_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_set_ad_sample_rate(codec, 48000, 0x0F, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static inline int adie_audio_codec_adie_loop_clk_en(
		struct snd_soc_codec *codec, int on)
{
	int ret = 0;

	if (on)
		ret = snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN),
					  BIT_CLK_AUD_LOOP_EN,
					  BIT_CLK_AUD_LOOP_EN);
	else
		ret = snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN),
					  BIT_CLK_AUD_LOOP_EN, 0);
	return ret;
}

static int adie_loop_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* enable loop clock */
		ret = adie_audio_codec_adie_loop_clk_en(codec, 1);
		snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
				    BIT_AUDIO_ADIE_LOOP_EN,
				    BIT_AUDIO_ADIE_LOOP_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* disable loop clock */
		ret = adie_audio_codec_adie_loop_clk_en(codec, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
				    BIT_AUDIO_ADIE_LOOP_EN, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int digital_loop_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	sprd_codec_set_ad_sample_rate(codec, 48000, 0x0F, 0);
	sprd_codec_set_sample_rate(codec, 48000, 0x0F, 0);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
				    BIT(LOOP_ADC_PATH_SEL),
				    BIT(LOOP_ADC_PATH_SEL));
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
				    BIT(AUD_LOOP_TEST), BIT(AUD_LOOP_TEST));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
				    BIT(AUD_LOOP_TEST), 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void hp_dpop_dvld_check(struct snd_soc_codec *codec, bool on_off,
			       unsigned int dvld_type)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	char type_str[sizeof("HP_LR_TYPE")] = "";
	int i = 0, state;

	strcpy(type_str, hp_chn_type_txt[sprd_codec->hp_chn_type]);
	while (i++ < 20) {
		state = snd_soc_read(codec, SOC_REG(ANA_STS1));

		/* if on, DVLD must be 1 */
		if (on_off && ((state & dvld_type) == dvld_type))
			break;
		/* if !on, DVLD must be 0 */
		else if (!on_off && ((state & dvld_type) == 0))
			break;

		/* check further */
		sprd_codec_wait(10);
	}

	sp_asoc_pr_info("%s %s DPOP %s, i %d, ANA_STS1 0x%x, dvld_type 0x%x\n",
			__func__, type_str, (i >= 20) ? "failed" : "sucessed",
			i, state, dvld_type);
}

static int hp_chn_en_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int sda_type, dpop_type, hp_en_type;
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s, type: %s\n", w->name,
			get_event_name(event),
			hp_chn_type_txt[sprd_codec->hp_chn_type]);

	switch (sprd_codec->hp_chn_type) {
	case HP_LR_TYPE:
		sda_type = SDALHPL_HPL | SDARHPR_HPR;
		dpop_type = HPL_DPOP_DVLD | HPR_DPOP_DVLD;
		hp_en_type = HPL_EN | HPR_EN;
		break;
	case HP_R_TYPE:
		sda_type = SDARHPR_HPR;
		dpop_type = HPR_DPOP_DVLD;
		hp_en_type = HPR_EN;
		break;
	case HP_L_TYPE:
		sda_type = SDALHPL_HPL;
		dpop_type = HPL_DPOP_DVLD;
		hp_en_type = HPL_EN;
		break;
	default:
		sda_type = SDALHPL_HPL | SDARHPR_HPR;
		dpop_type = HPL_DPOP_DVLD | HPR_DPOP_DVLD;
		hp_en_type = HPL_EN | HPR_EN;
		pr_err("%s hp chn type error, using HP_LR_TYPE\n", __func__);
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		update_switch(codec, sda_type, on);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC10),
				    hp_en_type, hp_en_type);
		hp_dpop_dvld_check(codec, on, dpop_type);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		update_switch(codec, sda_type, on);
		hp_dpop_dvld_check(codec, on, dpop_type);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC10),
				    hp_en_type, 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int cp_ad_cmp_cali_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
				    CP_AD_CMP_CAL_AVG(0xFFFF),
				    CP_AD_CMP_CAL_AVG(1));
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
				    CP_AD_CMP_AUTO_CAL_EN,
				    CP_AD_CMP_AUTO_CAL_EN);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL11),
				    CP_AD_CMP_AUTO_CAL_RANGE, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
				    CP_AD_EN, CP_AD_EN);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
				    CP_AD_CMP_CAL_EN, CP_AD_CMP_CAL_EN);
		sprd_codec_wait(3);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

/* check further, all regs needed here? */
static int cp_event(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u32 neg_cp = 0;
	int ret = 0;

	sp_asoc_pr_dbg("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CP positive power on */
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
				    CP_POS_SOFT_EN, CP_POS_SOFT_EN);
		/* CP negative power on */
		neg_cp = (sprd_codec->neg_cp_efuse >> 7) & 0xff;
		/* set negative voltalge to 1.62 by ASIC suggest */
		neg_cp = (neg_cp * 162) / 165;
		neg_cp = 0xe1;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL14),
				    CP_NEG_HV(0xFFFF), CP_NEG_HV(neg_cp));
		neg_cp = (neg_cp * 110) / 165;
		neg_cp = 0x9e;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL14),
				    CP_NEG_LV(0xFFFF), CP_NEG_LV(neg_cp));
		sp_asoc_pr_dbg("%s ANA_DCL14 0x%x\n", __func__,
			       arch_audio_codec_read(ANA_DCL14));

		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
				    CP_NEG_SOFT_EN, CP_NEG_SOFT_EN);
		/* wait 100us by guidline */
		usleep_range(100, 110);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU7),
			CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
				    CP_AD_EN, CP_AD_EN);
		usleep_range(50, 80);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0), CP_EN, CP_EN);
		sprd_codec_wait(1);

		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
				    CP_AD_EN | CP_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU7),
			CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP,
			CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
		break;
	}

	sp_asoc_pr_dbg("%s ret %d\n", __func__, ret);
	return ret;
}

static int dns_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("'%s' %s\n", w->name, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC11),
				    RG_AUD_HPL_G_BP_EN,
				    RG_AUD_HPL_G_BP_EN);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC11),
				    RG_AUD_HPR_G_BP_EN,
				    RG_AUD_HPR_G_BP_EN);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN),
				    RG_DNS_HPL_G(0xf),
				    RG_DNS_HPL_G(0x1));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN),
				    RG_DNS_HPR_G(0xf),
				    RG_DNS_HPR_G(0x1));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN),
				    RG_DNS_BYPASS, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_SW),
				    BIT(RG_DNS_SW),
				    BIT(RG_DNS_SW));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_SW),
				    RG_DNS_DG_SW,
				    RG_DNS_DG_SW);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_ZC_SW),
				    RG_DNS_ZC_SW,
				    RG_DNS_ZC_SW);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_MODE),
				    RG_DNS_PGA_TH_L(0xf),
				    RG_DNS_PGA_TH_L(0x1));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_MODE),
				    RG_DNS_PGA_TH_R(0xf),
				    RG_DNS_PGA_TH_R(0x1));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_MODE),
				    RG_DNS_MODE(0x3),
				    RG_DNS_MODE(0x3));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_GAIN_WT_SW),
				    RG_DNS_DG_GAIN_WT_SW,
				    RG_DNS_DG_GAIN_WT_SW);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_BLOCK_MS),
				    RG_DNS_BLOCK_MS(0xffff),
				    RG_DNS_BLOCK_MS(0x2));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_BLOCK_SMPL),
				    RG_DNS_MS_SMPL(0xffff),
				    RG_DNS_MS_SMPL(0xC0));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_M_HOLD_SET_F),
				    RG_DNS_M_HOLD_SET_F(0xffff),
				    RG_DNS_M_HOLD_SET_F(0x400));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_M_HOLD_SET_N),
				    RG_DNS_M_HOLD_SET_N(0xffff),
				    RG_DNS_M_HOLD_SET_N(0x400));

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_0),
				    RG_DNS_LEVEL_SET_0(0xffff),
				    RG_DNS_LEVEL_SET_0(0x7fff));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_1),
				    RG_DNS_LEVEL_SET_1(0xffff),
				    RG_DNS_LEVEL_SET_1(0x07cc));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_2),
				    RG_DNS_LEVEL_SET_2(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_3),
				    RG_DNS_LEVEL_SET_3(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_4),
				    RG_DNS_LEVEL_SET_4(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_5),
				    RG_DNS_LEVEL_SET_5(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_6),
				    RG_DNS_LEVEL_SET_6(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_7),
				    RG_DNS_LEVEL_SET_7(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_8),
				    RG_DNS_LEVEL_SET_8(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_9),
				    RG_DNS_LEVEL_SET_9(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_10),
				    RG_DNS_LEVEL_SET_10(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_11),
				    RG_DNS_LEVEL_SET_11(0xffff), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_LEVEL_SET_12),
				    RG_DNS_LEVEL_SET_12(0xffff), 0);

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_0),
				    RG_DNS_DG_GAIN_SET_0(0xffff),
				    RG_DNS_DG_GAIN_SET_0(0x0200));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_1),
				    RG_DNS_DG_GAIN_SET_1(0xffff),
				    RG_DNS_DG_GAIN_SET_1(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_2),
				    RG_DNS_DG_GAIN_SET_2(0xffff),
				    RG_DNS_DG_GAIN_SET_2(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_3),
				    RG_DNS_DG_GAIN_SET_3(0xffff),
				    RG_DNS_DG_GAIN_SET_3(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_4),
				    RG_DNS_DG_GAIN_SET_4(0xffff),
				    RG_DNS_DG_GAIN_SET_4(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_5),
				    RG_DNS_DG_GAIN_SET_5(0xffff),
				    RG_DNS_DG_GAIN_SET_5(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_6),
				    RG_DNS_DG_GAIN_SET_6(0xffff),
				    RG_DNS_DG_GAIN_SET_6(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_7),
				    RG_DNS_DG_GAIN_SET_7(0xffff),
				    RG_DNS_DG_GAIN_SET_7(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_8),
				    RG_DNS_DG_GAIN_SET_8(0xffff),
				    RG_DNS_DG_GAIN_SET_8(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_9),
				    RG_DNS_DG_GAIN_SET_9(0xffff),
				    RG_DNS_DG_GAIN_SET_9(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_10),
				    RG_DNS_DG_GAIN_SET_10(0xffff),
				    RG_DNS_DG_GAIN_SET_10(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_11),
				    RG_DNS_DG_GAIN_SET_11(0xffff),
				    RG_DNS_DG_GAIN_SET_11(0x2000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_SET_12),
				    RG_DNS_DG_GAIN_SET_12(0xffff),
				    RG_DNS_DG_GAIN_SET_12(0x2000));

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_VT_MAX),
				    RG_DNS_DG_GAIN_VT_MAX(0xf),
				    RG_DNS_DG_GAIN_VT_MAX(0xc));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_ZC_DG_UP_LAST_SET),
				    RG_DNS_ZC_DG_UP_LAST_SET(0xffff),
				    RG_DNS_ZC_DG_UP_LAST_SET(0x20));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_USTEP),
				    RG_DNS_DG_USTEP(0xf),
				    RG_DNS_DG_USTEP(0x8));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_ZC_DG_DN_C),
				    RG_DNS_ZC_DG_DN_C, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_ZC_PGA_C),
				    RG_DNS_ZC_PGA_C,
				    RG_DNS_ZC_PGA_C);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_QUICKEXTI_N),
				    RG_DNS_QUICKEXTI_N(0xf), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_DG_GAIN_DEFAULT),
				    RG_DNS_DG_GAIN_DEFAULT(0x1f), 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_MAX),
				    RG_DNS_PGA_MAX(0x3f),
				    RG_DNS_PGA_MAX(0xC));

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_0_L),
				    RG_DNS_PGA_GAIN_CAL_SET_0_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_0_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_1_L),
				    RG_DNS_PGA_GAIN_CAL_SET_1_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_1_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_2_L),
				    RG_DNS_PGA_GAIN_CAL_SET_2_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_2_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_3_L),
				    RG_DNS_PGA_GAIN_CAL_SET_3_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_3_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_4_L),
				    RG_DNS_PGA_GAIN_CAL_SET_4_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_4_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_5_L),
				    RG_DNS_PGA_GAIN_CAL_SET_5_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_5_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_6_L),
				    RG_DNS_PGA_GAIN_CAL_SET_6_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_6_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_7_L),
				    RG_DNS_PGA_GAIN_CAL_SET_7_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_7_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_8_L),
				    RG_DNS_PGA_GAIN_CAL_SET_8_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_8_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_9_L),
				    RG_DNS_PGA_GAIN_CAL_SET_9_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_9_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_10_L),
				    RG_DNS_PGA_GAIN_CAL_SET_10_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_10_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_11_L),
				    RG_DNS_PGA_GAIN_CAL_SET_11_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_11_L(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_12_L),
				    RG_DNS_PGA_GAIN_CAL_SET_12_L(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_12_L(0x1000));

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_0_L),
				    RG_DNS_PGA_DELAY_CAL_SET_0_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_0_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_1_L),
				    RG_DNS_PGA_DELAY_CAL_SET_1_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_1_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_2_L),
				    RG_DNS_PGA_DELAY_CAL_SET_2_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_2_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_3_L),
				    RG_DNS_PGA_DELAY_CAL_SET_3_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_3_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_4_L),
				    RG_DNS_PGA_DELAY_CAL_SET_4_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_4_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_5_L),
				    RG_DNS_PGA_DELAY_CAL_SET_5_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_5_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_6_L),
				    RG_DNS_PGA_DELAY_CAL_SET_6_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_6_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_7_L),
				    RG_DNS_PGA_DELAY_CAL_SET_7_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_7_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_8_L),
				    RG_DNS_PGA_DELAY_CAL_SET_8_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_8_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_9_L),
				    RG_DNS_PGA_DELAY_CAL_SET_9_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_9_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_10_L),
				    RG_DNS_PGA_DELAY_CAL_SET_10_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_10_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_11_L),
				    RG_DNS_PGA_DELAY_CAL_SET_11_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_11_L(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_12_L),
				    RG_DNS_PGA_DELAY_CAL_SET_12_L(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_12_L(0x1f));

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_0_R),
				    RG_DNS_PGA_GAIN_CAL_SET_0_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_0_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_1_R),
				    RG_DNS_PGA_GAIN_CAL_SET_1_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_1_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_2_R),
				    RG_DNS_PGA_GAIN_CAL_SET_2_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_2_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_3_R),
				    RG_DNS_PGA_GAIN_CAL_SET_3_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_3_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_4_R),
				    RG_DNS_PGA_GAIN_CAL_SET_4_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_4_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_5_R),
				    RG_DNS_PGA_GAIN_CAL_SET_5_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_5_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_6_R),
				    RG_DNS_PGA_GAIN_CAL_SET_6_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_6_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_7_R),
				    RG_DNS_PGA_GAIN_CAL_SET_7_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_7_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_8_R),
				    RG_DNS_PGA_GAIN_CAL_SET_8_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_8_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_9_R),
				    RG_DNS_PGA_GAIN_CAL_SET_9_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_9_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_10_R),
				    RG_DNS_PGA_GAIN_CAL_SET_10_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_10_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_11_R),
				    RG_DNS_PGA_GAIN_CAL_SET_11_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_11_R(0x1000));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_GAIN_CAL_SET_12_R),
				    RG_DNS_PGA_GAIN_CAL_SET_12_R(0x3fff),
				    RG_DNS_PGA_GAIN_CAL_SET_12_R(0x1000));

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_0_R),
				    RG_DNS_PGA_DELAY_CAL_SET_0_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_0_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_1_R),
				    RG_DNS_PGA_DELAY_CAL_SET_1_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_1_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_2_R),
				    RG_DNS_PGA_DELAY_CAL_SET_2_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_2_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_3_R),
				    RG_DNS_PGA_DELAY_CAL_SET_3_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_3_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_4_R),
				    RG_DNS_PGA_DELAY_CAL_SET_4_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_4_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_5_R),
				    RG_DNS_PGA_DELAY_CAL_SET_5_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_5_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_6_R),
				    RG_DNS_PGA_DELAY_CAL_SET_6_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_6_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_7_R),
				    RG_DNS_PGA_DELAY_CAL_SET_7_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_7_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_8_R),
				    RG_DNS_PGA_DELAY_CAL_SET_8_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_8_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_9_R),
				    RG_DNS_PGA_DELAY_CAL_SET_9_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_9_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_10_R),
				    RG_DNS_PGA_DELAY_CAL_SET_10_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_10_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_11_R),
				    RG_DNS_PGA_DELAY_CAL_SET_11_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_11_R(0x1f));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_PGA_DELAY_CAL_SET_12_R),
				    RG_DNS_PGA_DELAY_CAL_SET_12_R(0x1f),
				    RG_DNS_PGA_DELAY_CAL_SET_12_R(0x1f));

		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_CFG_LOAD_FLAG),
				    RG_DNS_CFG_LOAD_FLAG, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_CFG_LOAD_FLAG),
				    RG_DNS_CFG_LOAD_FLAG,
				    RG_DNS_CFG_LOAD_FLAG);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC11),
				    RG_AUD_HPL_G_BP_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC11),
				    RG_AUD_HPR_G_BP_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN),
				    RG_DNS_HPL_G(0xf),
				    RG_DNS_HPL_G(0x3));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN),
				    RG_DNS_HPR_G(0xf),
				    RG_DNS_HPR_G(0x3));
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN),
				    RG_DNS_BYPASS,
				    RG_DNS_BYPASS);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_SW),
				    BIT(RG_DNS_SW), 1);
		snd_soc_update_bits(codec, SOC_REG(AUD_DNS_SW),
				    BIT(RG_DNS_SW), 0);
		break;
	default:
		pr_err("%s invalid event error 0x%x\n", __func__, event);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#define REGU_CNT 2
static void cp_short_check(struct sprd_codec_priv *sprd_codec)
{
	struct snd_soc_codec *codec;
	const char *regu_id[REGU_CNT] = {"BG", "BIAS"};
	struct device *dev;
	struct regulator *regu[REGU_CNT];
	unsigned int regu_mode[REGU_CNT];
	int ret = 0;
	int i;
	int mask1, mask2;
	unsigned int idx;
	unsigned int flag;
	unsigned int val1[] = {
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYN,
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYN,
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYP,
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYP,
		CP_NEG_PD_FLYP | CP_NEG_PD_FLYN,
		CP_NEG_PD_FLYP | CP_NEG_PD_FLYN,
	};
	unsigned int val2[] = {
		CP_NEG_SHDT_FLYP_EN | CP_NEG_SHDT_PGSEL,
		CP_NEG_SHDT_FLYP_EN,
		CP_NEG_SHDT_FLYN_EN | CP_NEG_SHDT_PGSEL,
		CP_NEG_SHDT_FLYN_EN,
		CP_NEG_SHDT_VCPN_EN | CP_NEG_SHDT_PGSEL,
		CP_NEG_SHDT_VCPN_EN,
	};

	codec = sprd_codec ? sprd_codec->codec : NULL;
	if (!codec)
		return;
	dev = codec->dev;

	if (codec == NULL) {
		pr_err("%s, codec is NULL!", __func__);
		return;
	}

	/* Enable VB, BG & BIAS */
	for (i = 0; i < REGU_CNT; i++) {
		regu[i] = regulator_get(dev, regu_id[i]);
		if (IS_ERR(regu[i])) {
			pr_err("error, Failed to request %ld %s\n",
			       PTR_ERR(regu[i]), regu_id[i]);
			goto REGULATOR_CLEAR;
		}
		ret = regulator_enable(regu[i]);
		if (ret) {
			pr_err("%s, regulator_enable failed", __func__);
			regulator_put(regu[i]);
			goto REGULATOR_CLEAR;
		}
		regu_mode[i] = regulator_get_mode(regu[i]);
		regulator_set_mode(regu[i], REGULATOR_MODE_NORMAL);
	}
	usleep_range(100, 150);

	snd_soc_update_bits(codec, SOC_REG(ANA_PMU7),
			    CP_NEG_CLIMIT_EN, CP_NEG_CLIMIT_EN);
	usleep_range(50, 100);

	mask1 = CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP;
	mask2 = CP_NEG_SHDT_VCPN_EN | CP_NEG_SHDT_FLYP_EN |
		CP_NEG_SHDT_FLYN_EN | CP_NEG_SHDT_PGSEL;
	for (idx = 0; idx < ARRAY_SIZE(val1); idx++) {
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU7), mask1, val1[idx]);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU8), mask2, val2[idx]);
		sprd_codec_wait(20);
		flag = snd_soc_read(codec, SOC_REG(ANA_STS5)) &
			BIT(CP_NEG_SH_FLAG);
		if (flag) {
			sprd_codec->cp_short_stat = idx + 1;
			break;
		}
	}
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU7), mask1, mask1);
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU8), mask2, 0);

	/* Disable VB, BG & BIAS */
REGULATOR_CLEAR:
	for (i -= 1; i >= 0; i--) {
		regulator_set_mode(regu[i], regu_mode[i]);
		regulator_disable(regu[i]);
		regulator_put(regu[i]);
	}
}

static const char * const hpl_rcv_mux_texts[] = {
	"EAR", "HPL"
};

static const SOC_ENUM_SINGLE_VIRT_DECL(hpl_rcv_enum, hpl_rcv_mux_texts);
static const struct snd_kcontrol_new hpl_rcv_mux =
	SOC_DAPM_ENUM("HPL EAR Sel", hpl_rcv_enum);

static const char * const aol_ear_mux_texts[] = {
	"AOL", "EAR"
};

static const SOC_ENUM_SINGLE_VIRT_DECL(aol_ear_enum, aol_ear_mux_texts);
static const struct snd_kcontrol_new aol_ear_mux =
	SOC_DAPM_ENUM("AOL EAR Sel", aol_ear_enum);

#define SPRD_CODEC_MUX_KCTL(xname, xkctl, xreg, xshift, xtext) \
	static const SOC_ENUM_SINGLE_DECL(xkctl##_enum, xreg, xshift, xtext); \
	static const struct snd_kcontrol_new xkctl = \
		SOC_DAPM_ENUM_EXT(xname, xkctl##_enum, \
		snd_soc_dapm_get_enum_double, \
		snd_soc_dapm_put_enum_double)

static const char * const dig_adc_in_texts[] = {
	"ADC", "DAC", "DMIC"
};

SPRD_CODEC_MUX_KCTL("Digital ADC In Sel", dig_adc_in_sel,
		    AUD_TOP_CTL, ADC_SINC_SEL, dig_adc_in_texts);
SPRD_CODEC_MUX_KCTL("Digital ADC1 In Sel", dig_adc1_in_sel,
		    AUD_TOP_CTL, ADC1_SINC_SEL, dig_adc_in_texts);

static const struct snd_soc_dapm_widget sprd_codec_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("Digital Power", 1, SND_SOC_NOPM,
			      0, 0, digital_power_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("Analog Power", 0, SND_SOC_NOPM,
			      0, 0, analog_power_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_REGULATOR_SUPPLY("BG", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("BIAS", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS1", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS2", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS3", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("HEADMICBIAS", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("CP_LDO", 2, SOC_REG(ANA_PMU0),
			      CP_LDO_EN_S, 0, NULL, 0),

/* Clock */
	SND_SOC_DAPM_SUPPLY_S("ANA_CLK", 1, SOC_REG(ANA_CLK0),
			      ANA_CLK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK 26M IN SEL", 1, SND_SOC_NOPM,
			      0, 0, clk_26m_in_sel_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CLK_DCL_6M5", 1, SOC_REG(ANA_CLK0),
			      CLK_DCL_6M5_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("CLK_DCL_32K", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_CP", 1, SOC_REG(ANA_CLK0),
			      CLK_CP_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_CP_32K", 1, SOC_REG(ANA_CLK0),
			      CLK_CP_32K_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_IMPD", 1, SOC_REG(ANA_CLK0),
			      CLK_IMPD_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_DAC", 1, SOC_REG(ANA_CLK0),
			      CLK_DAC_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_ADC", 1, SOC_REG(ANA_CLK0),
			      CLK_ADC_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_DIG_6M5", 1, SOC_REG(ANA_CLK0),
			      CLK_DIG_6M5_EN_S, 0, NULL, 0),
/* DCL clock */
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_DAC_BUF", 4, SOC_REG(ANA_DCL1),
			      DIG_CLK_DAC_BUF_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("DCL", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_DRV_SOFT", 5, SOC_REG(ANA_DCL1),
			      DIG_CLK_DRV_SOFT_EN_S, 0,
			      audio_dcl_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_REGULATOR_SUPPLY("DIG_CLK_INTC", 0, 0),

	SND_SOC_DAPM_REGULATOR_SUPPLY("DIG_CLK_HID", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_CFGA_PRT", 5, SOC_REG(ANA_DCL1),
			      DIG_CLK_CFGA_PRT_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_HPDPOP", 5, SOC_REG(ANA_DCL1),
			      DIG_CLK_HPDPOP_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_IMPD", 5, SOC_REG(ANA_DCL1),
			      DIG_CLK_IMPD_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_RCV", 5, SOC_REG(ANA_DCL1),
			      DIG_CLK_RCV_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DALDO EN", 2, SND_SOC_NOPM,
			      0, 0, daldo_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("SDM DC OS", SPRD_CODEC_DC_OS_SWITCH_ORDER,
			      SND_SOC_NOPM, 0, 0, sdm_dc_os_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

/* ADC CLK */
	SND_SOC_DAPM_SUPPLY_S("ADC_1_CLK", 5, SOC_REG(ANA_CDC0),
			      ADC_1_CLK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC_2_CLK", 5, SOC_REG(ANA_CDC0),
			      ADC_2_CLK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC_3_CLK", 5, SOC_REG(ANA_CDC0),
			      ADC_3_CLK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC_IBIAS", 3, SOC_REG(ANA_CDC0),
			      PGA_ADC_IBIAS_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC_VREF_BUF", 3, SOC_REG(ANA_CDC0),
			      PGA_ADC_VCM_VREF_BUF_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("IVSense SRC", 3, SND_SOC_NOPM, 0, 0,
			      sprd_ivsense_src_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

/* AD route */
	SND_SOC_DAPM_MIXER("ADC_1 Mixer", SND_SOC_NOPM, 0, 0,
			   &adc_1_mixer_controls[0],
			   ARRAY_SIZE(adc_1_mixer_controls)),
	SND_SOC_DAPM_MIXER("ADC_2 Mixer", SND_SOC_NOPM, 0, 0,
			   &adc_2_mixer_controls[0],
			   ARRAY_SIZE(adc_2_mixer_controls)),
	SND_SOC_DAPM_MIXER("ADC_3 Mixer", SND_SOC_NOPM, 0, 0,
			   &adc_3_mixer_controls[0],
			   ARRAY_SIZE(adc_3_mixer_controls)),
	SND_SOC_DAPM_SWITCH("AUD ADC0L", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[0]),
	SND_SOC_DAPM_SWITCH("AUD ADC0R", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[1]),
	SND_SOC_DAPM_SWITCH("AUD ADC1L", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[2]),
	SND_SOC_DAPM_SWITCH("AUD ADC1R", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[3]),

	SND_SOC_DAPM_PGA_E("ADC_1 Gain", SOC_REG(ANA_CDC0), ADPGA_1_EN_S, 0,
			   adc_1_pga_controls, 1, NULL, 0),
	SND_SOC_DAPM_PGA_E("ADC_2 Gain", SOC_REG(ANA_CDC0), ADPGA_2_EN_S, 0,
			   adc_2_pga_controls, 1, NULL, 0),
	SND_SOC_DAPM_PGA_E("ADC_3 Gain", SOC_REG(ANA_CDC0), ADPGA_3_EN_S, 0,
			   adc_3_pga_controls, 1, NULL, 0),
	SND_SOC_DAPM_PGA_E("ADC_1 Switch", SOC_REG(ANA_CDC0),
			   ADC_1_EN_S, 0, 0, 0, adc_1_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("ADC_2 Switch", SOC_REG(ANA_CDC0),
			   ADC_2_EN_S, 0, 0, 0, adc_2_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("ADC_3 Switch", SOC_REG(ANA_CDC0),
			   ADC_3_EN_S, 0, 0, 0, adc_3_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_PGA_S("ADie Digital ADCL Switch", 2,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
			   BIT_ADC_EN_L_S, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCR Switch", 2,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
			   BIT_ADC_EN_R_S, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADCL Switch", 4, SOC_REG(AUD_TOP_CTL),
			   ADC_EN_L, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADCR Switch", 4, SOC_REG(AUD_TOP_CTL),
			   ADC_EN_R, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADC1L Switch", 4, SOC_REG(AUD_TOP_CTL),
			   ADC1_EN_L, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADC1R Switch", 4, SOC_REG(AUD_TOP_CTL),
			   ADC1_EN_R, 0, 0, 0),
	SND_SOC_DAPM_ADC_E("ADC", "Normal-Capture-AP01",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1", "Normal-Capture-AP23",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC AP23", "Normal-Capture-AP23",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC DSP_C", "Capture-DSP",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC DSP_C_btsco_test", "Capture-DSP-BTSCO-test",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC DSP_C_fm_test", "Capture-DSP-FM-test",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC Voip", "Voip-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC CODEC_TEST", "CODEC_TEST-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE),	0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC LOOP", "LOOP-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC Voice", "Voice-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("Capture RECOGNISE", "Capture-DSP-RECOGNISE",
			   FUN_REG(SPRD_CODEC_CAPTRUE), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
/* DA route */
	SND_SOC_DAPM_DAC_E("DAC", "Normal-Playback-AP01",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC AP23", "Normal-Playback-AP23",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Voice", "Voice-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Offload", "Offload-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fast", "Fast-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Voip", "Voip-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC CODEC_TEST", "CODEC_TEST-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC LOOP", "LOOP-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fm", "Fm-Playback",
			   SND_SOC_NOPM, 0, 0, dfm_out_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fm_DSP", "Fm-DSP-Playback",
			   SND_SOC_NOPM, 0, 0, dfm_out_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Digital DACL Switch", -4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_L, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital DACR Switch", -4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_R, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACL Switch", -3,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
			   BIT_DAC_EN_L_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACR Switch", -3,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
			   BIT_DAC_EN_R_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_E("DAC Virt", SND_SOC_NOPM, 0, 0, NULL, 0,
			   codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DAC Gain", SND_SOC_NOPM, 0, 0,
			   dac_pga_controls, 1, 0, 0),
	SND_SOC_DAPM_PGA_S("DRV SOFT EN", 100,
			   SOC_REG(ANA_CDC12), DRV_SOFT_EN_S, 0, NULL, 0),

/* SPK */
	SND_SOC_DAPM_PGA_S("DRV SOFT T", 5, SND_SOC_NOPM,
			   0, 0, drv_soft_t_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_S("DAAOL BUF DCCAL", SPRD_CODEC_DA_BUF_DCCAL_ORDER,
			   SND_SOC_NOPM, 0, 0,
			   daaol_buf_cal_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("DAAOR BUF DCCAL", SPRD_CODEC_DA_BUF_DCCAL_ORDER,
			   SND_SOC_NOPM, 0, 0,
			   daaor_buf_cal_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_S("DAAO OS", SPRD_CODEC_DC_OS_ORDER,
			   SND_SOC_NOPM, 0, 0,
			   daao_os_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("AO Gain", SND_SOC_NOPM, 0, 0,
			   ao_gain_controls, 1, 0, 0),
	SND_SOC_DAPM_PGA_E("AO Virt", SND_SOC_NOPM, 0, 0, NULL, 0,
			   codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("AO Virt2", SND_SOC_NOPM, 0, 0, NULL, 0,
			   codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	/* check further, temp widget */
	SND_SOC_DAPM_PGA_E("AOL Virt", SND_SOC_NOPM, 0, 0, NULL, 0,
			   codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("AO Mixer", SND_SOC_NOPM, 0, 0,
			   &dacao_mixer_controls[0],
			   ARRAY_SIZE(dacao_mixer_controls)),
	SND_SOC_DAPM_PGA_E("AO CUR", SOC_REG(ANA_CDC14), AO_CUR_S, 0,
			   NULL, 0, codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("SPK PA", SND_SOC_NOPM, 0, 0,
			   NULL, 0,
			   codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

/* AOL and EAR DEMUX */
	/* kcontrol "AOL EAR Sel" to "AOL" or "EAR" */
	SND_SOC_DAPM_DEMUX("AOL EAR Sel", SND_SOC_NOPM, 0, 0, &aol_ear_mux),
	SND_SOC_DAPM_DEMUX("AOL EAR Sel2", SND_SOC_NOPM, 0, 0, &aol_ear_mux),

	SND_SOC_DAPM_PGA_S("DAAOL EN", SPRD_CODEC_DA_EN_ORDER,
			   SOC_REG(ANA_DAC2), DAAOL_EN_S, 0, codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("DAAOR EN", SPRD_CODEC_DA_EN_ORDER,
			   SOC_REG(ANA_DAC2), DAAOR_EN_S, 0, codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SWITCH_E("DA AOL", SND_SOC_NOPM,
			      0, 0, &dac_ao_hp_switch[0], codec_widget_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("DA AOR", SND_SOC_NOPM,
			      0, 0, &dac_ao_hp_switch[1], codec_widget_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("DA HPL", SND_SOC_NOPM,
			      0, 0, &dac_ao_hp_switch[2], codec_widget_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("DA HPR", SND_SOC_NOPM, 0, 0,
			      &dac_ao_hp_switch[3], codec_widget_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("AO EN", SPRD_CODEC_AO_EN_ORDER,
			   SOC_REG(ANA_CDC10), AO_EN_S, 0,
			   codec_widget_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SDAAOL AO", SPRD_CODEC_SDA_EN_ORDER,
			   SND_SOC_NOPM, 0, 0,
			   sdaaol_ao_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SDAAOR AO", SPRD_CODEC_SDA_EN_ORDER,
			   SND_SOC_NOPM, 0, 0,
			   sdaaor_ao_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SDAAOL RCV", SPRD_CODEC_SDA_EN_ORDER,
			   SND_SOC_NOPM, 0, 0,
			   sdaaol_rcv_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SDAHPL RCV", SPRD_CODEC_SDA_EN_ORDER,
			   SND_SOC_NOPM, 0, 0,
			   sdahpl_rcv_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

/* HP */
	SND_SOC_DAPM_PGA_S("HPRCV COM Virt", SPRD_CODEC_DA_EN_ORDER,
			   SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA_S("DAHPL EN", SPRD_CODEC_DA_EN_ORDER,
			   SOC_REG(ANA_DAC1), DAHPL_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DAHPR EN", SPRD_CODEC_DA_EN_ORDER,
			   SOC_REG(ANA_DAC1), DAHPR_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DAHP OS", SPRD_CODEC_DC_OS_ORDER,
			   SND_SOC_NOPM, 0, 0, dahp_os_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("DNS", SPRD_CODEC_DC_OS_ORDER,
				SND_SOC_NOPM, 0, 0, dns_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*
	 * don't do cali on AA chip
	 * choose one from following(do not choose both):
	 * option 1. do cali(For not AA chip or need cali):
	 * kcontrol "DAHPL BUF DCCAL" --> "HW_CAL_MODE"
	 * kcontrol "DAHPR BUF DCCAL" --> "HW_CAL_MODE"
	 *
	 * option 2. not do cali(For AA chip or needn't cali):
	 * kcontrol "DAHPL BUF DCCAL" --> "SW_CAL_MODE" or NOT_CAL
	 * kcontrol "DAHPR BUF DCCAL" --> "SW_CAL_MODE" or NOT_CAL
	 */
	SND_SOC_DAPM_PGA_S("DAHPL BUF DCCAL", SPRD_CODEC_DA_BUF_DCCAL_ORDER,
			   SND_SOC_NOPM, 0, 0, dahpl_buf_cal_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("DAHPR BUF DCCAL", SPRD_CODEC_DA_BUF_DCCAL_ORDER,
			   SND_SOC_NOPM, 0, 0, dahpr_buf_cal_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_E("HPL Gain", SND_SOC_NOPM, 0, 0, hpl_pga_controls, 1,
			   0, 0),
	SND_SOC_DAPM_PGA_E("HPR Gain", SND_SOC_NOPM, 0, 0, hpr_pga_controls, 1,
			   0, 0),
	SND_SOC_DAPM_MIXER("HPL Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_mixer_controls[0],
			   ARRAY_SIZE(hpl_mixer_controls)),
	SND_SOC_DAPM_MIXER("HPR Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_mixer_controls[0],
			   ARRAY_SIZE(hpr_mixer_controls)),
	SND_SOC_DAPM_PGA_S("HP DEPOP", SPRD_CODEC_DEPOP_ORDER, SND_SOC_NOPM,
			   0, 0, hp_depop_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HP CHN EN", SPRD_CODEC_HP_EN_ORDER,
			   SND_SOC_NOPM, 0, 0, hp_chn_en_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY_S("CP AD Cali", 5, SND_SOC_NOPM, 0, 0,
			      cp_ad_cmp_cali_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CP", 4, SND_SOC_NOPM, 0, 0, cp_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

/* EAR */
	/* check further the wsubseq number */
	SND_SOC_DAPM_PGA_S("RCV DEPOP", 5, SOC_REG(ANA_DBG4),
			   RCV_DPOP_EN_S, 0, codec_widget_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_E("EAR Gain", SND_SOC_NOPM, 0, 0,
			   ear_pga_controls, 1, 0, 0),
	SND_SOC_DAPM_MIXER("EAR Mixer", SND_SOC_NOPM, 0, 0,
			   &ear_mixer_controls[0],
			   ARRAY_SIZE(ear_mixer_controls)),
	SND_SOC_DAPM_PGA_S("EAR Switch", SPRD_CODEC_RCV_EN_ORDER,
			   SND_SOC_NOPM, 0, 0, ear_switch_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("FDIN DVLD", SPRD_CODEC_RCV_DEPOP_ORDER,
			   SND_SOC_NOPM, 0, 0, fdin_dvld_check_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

/* HP and EAR DEMUX */
	SND_SOC_DAPM_DEMUX("HPL EAR Sel", SND_SOC_NOPM, 0, 0, &hpl_rcv_mux),
	SND_SOC_DAPM_DEMUX("HPL EAR Sel2", SND_SOC_NOPM, 0, 0, &hpl_rcv_mux),
	SND_SOC_DAPM_PGA_E("HPL Path", SND_SOC_NOPM, 0, 0, NULL, 0,
			   codec_widget_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_PGA_E("EAR_HPL Path", SND_SOC_NOPM, 0, 0, NULL, 0,
			   codec_widget_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_PGA_E("EAR_AOL Path", SND_SOC_NOPM, 0, 0, NULL, 0,
			   codec_widget_event, SND_SOC_DAPM_PRE_PMU),

/* PIN */
	SND_SOC_DAPM_OUTPUT("EAR Pin"),
	SND_SOC_DAPM_OUTPUT("HP Pin"),
	SND_SOC_DAPM_OUTPUT("SPK Pin"),

	SND_SOC_DAPM_OUTPUT("Virt Output Pin"),
	SND_SOC_DAPM_SWITCH("Virt Output", SND_SOC_NOPM,
			    0, 0, &virt_output_switch),

	SND_SOC_DAPM_INPUT("MIC Pin"),
	SND_SOC_DAPM_INPUT("MIC2 Pin"),
	SND_SOC_DAPM_INPUT("MIC3 Pin"),
	SND_SOC_DAPM_INPUT("HPMIC Pin"),

	SND_SOC_DAPM_INPUT("DMIC Pin"),
	SND_SOC_DAPM_INPUT("DMIC1 Pin"),

	/* add DMIC */
	SND_SOC_DAPM_PGA_S("DMIC Switch", 3, SOC_REG(AUD_DMIC_CTL),
			   ADC_DMIC_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DMIC1 Switch", 3, SOC_REG(AUD_DMIC_CTL),
			   ADC1_DMIC1_EN, 0, NULL, 0),
	SND_SOC_DAPM_SWITCH("ADC-DAC Adie Loop",
			    SND_SOC_NOPM, 0, 0, &loop_controls[0]),
	SND_SOC_DAPM_SWITCH("ADC1-DAC Adie Loop",
			    SND_SOC_NOPM, 0, 0, &loop_controls[1]),
	SND_SOC_DAPM_SWITCH("ADC-DAC Digital Loop",
			    SND_SOC_NOPM, 0, 0, &loop_controls[2]),
	SND_SOC_DAPM_SWITCH("ADC1-DAC Digital Loop",
			    SND_SOC_NOPM, 0, 0, &loop_controls[3]),
	SND_SOC_DAPM_PGA_S("ADC-DAC Adie Loop post",
			   SPRD_CODEC_ANA_MIXER_ORDER,
			   SND_SOC_NOPM, 0, 0, adie_loop_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADC-DAC Digital Loop post",
			   SPRD_CODEC_ANA_MIXER_ORDER,
			   SND_SOC_NOPM, 0, 0, digital_loop_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* for HP power up switch*/
	SND_SOC_DAPM_SWITCH("DA mode", SND_SOC_NOPM, 0, 1, &da_mode_switch),

	SND_SOC_DAPM_MUX("Digital ADC In Sel", SND_SOC_NOPM, 0, 0,
			 &dig_adc_in_sel),
	SND_SOC_DAPM_MUX("Digital ADC1 In Sel", SND_SOC_NOPM, 0, 0,
			 &dig_adc1_in_sel),
};

/* sprd_codec supported interconnection */
static const struct snd_soc_dapm_route sprd_codec_intercon[] = {
/* power and clock */
	{"Analog Power", NULL, "BIAS"},
	{"ANA_CLK", NULL, "Analog Power"},
	{"CLK 26M IN SEL", NULL, "ANA_CLK"},
	{"CLK_DCL_32K", NULL, "CLK 26M IN SEL"},
	{"CLK_DCL_6M5", NULL, "CLK_DCL_32K"},
	{"CLK_DIG_6M5", NULL, "CLK_DCL_6M5"},
	{"ADC_IBIAS", NULL, "ADC_VREF_BUF"},

	{"CLK_DAC", NULL, "CLK_DIG_6M5"},
	{"CLK_DAC", NULL, "DCL"},
	/* check further, lineout not use, any negative effects? */
	{"CLK_DAC", NULL, "DIG_CLK_DRV_SOFT"},
	{"CLK_DAC", NULL, "Digital Power"},
	{"CLK_DAC", NULL, "BG"},
	{"CLK_DAC", NULL, "DIG_CLK_DAC_BUF"},
	{"CLK_CP", NULL, "DALDO EN"},
	{"CP_LDO", NULL, "CLK_CP"},
	{"CP", NULL, "CP_LDO"},
	{"CLK_CP_32K", NULL, "CP"},
	{"CP AD Cali", NULL, "CLK_CP_32K"},
	{"CLK_DAC", NULL, "CP AD Cali"},

	{"CLK_ADC", NULL, "CLK_DIG_6M5"},
	{"CLK_ADC", NULL, "DCL"},
	{"CLK_ADC", NULL, "DIG_CLK_DRV_SOFT"},
	{"CLK_ADC", NULL, "Digital Power"},
	{"CLK_ADC", NULL, "ADC_IBIAS"},

/* DA route */
	{"DAC", NULL, "CLK_DAC"},
	{"DAC Voice", NULL, "CLK_DAC"},
	{"DAC Offload", NULL, "CLK_DAC"},
	{"DAC Fast", NULL, "CLK_DAC"},
	{"DAC Voip", NULL, "CLK_DAC"},
	{"DAC Fm", NULL, "CLK_DAC"},
	{"DAC Fm_DSP", NULL, "CLK_DAC"},
	{"DAC AP23", NULL, "CLK_DAC"},
	{"DAC CODEC_TEST", NULL, "CLK_DAC"},
	{"DAC LOOP", NULL, "CLK_DAC"},

	{"Digital DACL Switch", NULL, "DAC"},
	{"Digital DACR Switch", NULL, "DAC"},
	{"Digital DACL Switch", NULL, "DAC Fm"},
	{"Digital DACR Switch", NULL, "DAC Fm"},
	{"Digital DACL Switch", NULL, "DAC Fm_DSP"},
	{"Digital DACR Switch", NULL, "DAC Fm_DSP"},
	{"Digital DACL Switch", NULL, "DAC AP23"},
	{"Digital DACR Switch", NULL, "DAC AP23"},
	{"Digital DACL Switch", NULL, "DAC Voice"},
	{"Digital DACR Switch", NULL, "DAC Voice"},
	{"Digital DACL Switch", NULL, "DAC Offload"},
	{"Digital DACR Switch", NULL, "DAC Offload"},
	{"Digital DACL Switch", NULL, "DAC Fast"},
	{"Digital DACR Switch", NULL, "DAC Fast"},
	{"Digital DACL Switch", NULL, "DAC Voip"},
	{"Digital DACR Switch", NULL, "DAC Voip"},
	{"Digital DACL Switch", NULL, "DAC CODEC_TEST"},
	{"Digital DACR Switch", NULL, "DAC CODEC_TEST"},
	{"Digital DACL Switch", NULL, "DAC LOOP"},
	{"Digital DACR Switch", NULL, "DAC LOOP"},

	{"ADie Digital DACL Switch", NULL, "Digital DACL Switch"},
	{"ADie Digital DACR Switch", NULL, "Digital DACR Switch"},
	{"DAC Virt", NULL, "ADie Digital DACL Switch"},
	{"DAC Virt", NULL, "ADie Digital DACR Switch"},
	{"DRV SOFT EN", NULL, "DAC Virt"},
	{"DAC Gain", NULL, "DRV SOFT EN"},

/* SPK */
	{"DRV SOFT T", NULL, "DAC Gain"},
	{"DAAO OS", NULL, "DRV SOFT T"},
	{"AO Virt", NULL, "DAAO OS"},

	{"AO Mixer", "AOR Switch", "AO Virt"},/* AOR not in demux */
	{"DAAOR EN", NULL, "AO Mixer"},
	{"DAAOR BUF DCCAL", NULL, "DAAOR EN"},
	{"DA AOR", "Switch", "DAAOR BUF DCCAL"},
	{"SDAAOR AO", NULL, "DA AOR"},
	{"AO Virt2", NULL, "SDAAOR AO"},

	{"AO Mixer", "AOL Switch", "AO Virt"},
	{"DAAOL EN", NULL, "AO Mixer"},
	{"DAAOL BUF DCCAL", NULL, "DAAOL EN"},
	{"AOL EAR Sel", NULL, "DAAOL BUF DCCAL"},
	{"AOL Virt", "AOL", "AOL EAR Sel"},/* AOL is in demux */
	{"AOL EAR Sel2", NULL, "AOL Virt"},

	{"SDAAOL AO", "AOL", "AOL EAR Sel2"},
	{"AO Virt2", NULL, "SDAAOL AO"},

	{"AO Gain", NULL, "AO Virt2"},
	{"AO CUR", NULL, "AO Gain"},
	{"AO EN", NULL, "AO CUR"},
	{"SPK PA", NULL, "AO EN"},
	{"SPK PA", NULL, "DIG_CLK_RCV"},/* check further, enable rcv in spk? */
	{"SPK Pin", NULL, "SPK PA"},

/* HP */
	{"HP DEPOP", NULL, "DIG_CLK_HPDPOP"},
	{"HP DEPOP", NULL, "DIG_CLK_INTC"},
	{"HP DEPOP", NULL, "SDM DC OS"},
	{"HPRCV COM Virt", NULL, "DAC Gain"},
	{"DAHP OS", NULL, "HPRCV COM Virt"},
	//{"DAHP OS", NULL, "DNS"},

	{"DAHPL EN", NULL, "DAHP OS"},
	{"DAHPL BUF DCCAL", NULL, "DAHPL EN"},
	{"HPL EAR Sel", NULL, "DAHPL BUF DCCAL"},
	{"HPL Path", "HPL", "HPL EAR Sel"},
	{"HPL Mixer", "DACLHPL Switch", "HPL Path"},
	{"HPL EAR Sel2", NULL, "HPL Mixer"},/* HPL */
	{"HPL Gain", "HPL", "HPL EAR Sel2"},
	{"HP DEPOP", NULL, "HPL Gain"},

	{"DAHPR EN", NULL, "DAHP OS"},
	{"DAHPR BUF DCCAL", NULL, "DAHPR EN"},
	{"HPR Mixer", "DACRHPR Switch", "DAHPR BUF DCCAL"},
	{"HPR Gain", NULL, "HPR Mixer"},/* HPR */
	{"HP DEPOP", NULL, "HPR Gain"},

	{"HP CHN EN", NULL, "HP DEPOP"},
	{"HP Pin", NULL, "HP CHN EN"},

/* EAR */
	{"EAR Mixer", NULL, "DIG_CLK_INTC"},
	{"EAR Mixer", NULL, "DIG_CLK_RCV"},
	{"HPL EAR Sel", NULL, "DAC Gain"},
	{"AOL EAR Sel", NULL, "DAC Gain"},

	{"EAR_HPL Path", "EAR", "HPL EAR Sel"},
	{"EAR Mixer", "DACHPL Switch", "EAR_HPL Path"},
	{"SDAHPL RCV", NULL, "EAR Mixer"},
	{"HPL EAR Sel2", NULL, "SDAHPL RCV"},
	{"EAR Switch", "EAR", "HPL EAR Sel2"},

	{"EAR_AOL Path", "EAR", "AOL EAR Sel"},
	{"EAR Mixer", "DACAOL Switch", "EAR_AOL Path"},
	{"SDAAOL RCV", NULL, "EAR Mixer"},
	{"AOL EAR Sel2", NULL, "SDAAOL RCV"},
	{"EAR Switch", "EAR", "AOL EAR Sel2"},

	{"RCV DEPOP", NULL, "EAR Switch"},
	{"FDIN DVLD", NULL, "RCV DEPOP"},
	{"EAR Gain", NULL, "FDIN DVLD"},
	{"EAR Pin", NULL, "EAR Gain"},

/* AD route */
	{"ADC_1 Switch", NULL, "ADC_1_CLK"},
	{"ADC_2 Switch", NULL, "ADC_2_CLK"},
	{"ADC_3 Switch", NULL, "ADC_3_CLK"},
	{"ADC_1 Gain", NULL, "ADC_1 Mixer"},
	{"ADC_2 Gain", NULL, "ADC_2 Mixer"},
	{"ADC_3 Gain", NULL, "ADC_3 Mixer"},
	{"ADC_1 Switch", NULL, "ADC_1 Gain"},
	{"ADC_2 Switch", NULL, "ADC_2 Gain"},
	{"ADC_3 Switch", NULL, "ADC_3 Gain"},
	/*
	 * Nate.wei: ADC_1 ADC_2 --> AUD_ADD0
	 * ADC_3 (ADC_4 in future)  --> AUD_ADD1
	 * ADC_3 and VAD can't work at the same time
	 * aud digital: ADC_1 ADC_3 --> adc_en_l
	 * ADC_2 (ADC_4 in future) --> adc_en_r
	 */
	{"ADie Digital ADCL Switch", NULL, "ADC_1 Switch"},
	{"ADie Digital ADCR Switch", NULL, "ADC_2 Switch"},
	{"ADie Digital ADCL Switch", NULL, "ADC_3 Switch"},
	{"Digital ADC In Sel", "ADC", "ADie Digital ADCL Switch"},
	{"Digital ADC In Sel", "ADC", "ADie Digital ADCR Switch"},
	{"Digital ADC1 In Sel", "ADC", "ADie Digital ADCL Switch"},
	{"Digital ADC1 In Sel", "ADC", "ADie Digital ADCR Switch"},

	{"Digital ADCL Switch", NULL, "Digital ADC In Sel"},
	{"Digital ADCR Switch", NULL, "Digital ADC In Sel"},
	{"Digital ADC1L Switch", NULL, "Digital ADC1 In Sel"},
	{"Digital ADC1R Switch", NULL, "Digital ADC1 In Sel"},

	{"AUD ADC0L", "Switch", "Digital ADCL Switch"},
	{"AUD ADC0R", "Switch", "Digital ADCR Switch"},
	{"AUD ADC1L", "Switch", "Digital ADC1L Switch"},
	{"AUD ADC1R", "Switch", "Digital ADC1R Switch"},

	/* AUD ADC0 */
	{"ADC DSP_C", NULL, "AUD ADC0L"},
	{"ADC DSP_C", NULL, "AUD ADC0R"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC0L"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC0R"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC0L"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC0R"},
	{"ADC Voice", NULL, "AUD ADC0L"},
	{"ADC Voice", NULL, "AUD ADC0R"},
	{"Capture RECOGNISE", NULL, "AUD ADC0L"},
	{"Capture RECOGNISE", NULL, "AUD ADC0R"},
	{"ADC Voip", NULL, "AUD ADC0L"},
	{"ADC Voip", NULL, "AUD ADC0R"},
	{"ADC CODEC_TEST", NULL, "AUD ADC0L"},
	{"ADC CODEC_TEST", NULL, "AUD ADC0R"},
	{"ADC LOOP", NULL, "AUD ADC0L"},
	{"ADC LOOP", NULL, "AUD ADC0R"},
	{"ADC", NULL, "AUD ADC0L"},
	{"ADC", NULL, "AUD ADC0R"},
	{"ADC AP23", NULL, "AUD ADC0L"},
	{"ADC AP23", NULL, "AUD ADC0R"},
	{"ADC1", NULL, "AUD ADC0L"},
	{"ADC1", NULL, "AUD ADC0R"},

	/* AUD ADC1 */
	{"ADC DSP_C", NULL, "AUD ADC1L"},
	{"ADC DSP_C", NULL, "AUD ADC1R"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC1L"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC1R"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC1L"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC1R"},
	{"ADC Voice", NULL, "AUD ADC1L"},
	{"ADC Voice", NULL, "AUD ADC1R"},
	{"Capture RECOGNISE", NULL, "AUD ADC1L"},
	{"Capture RECOGNISE", NULL, "AUD ADC1R"},
	{"ADC Voip", NULL, "AUD ADC1L"},
	{"ADC Voip", NULL, "AUD ADC1R"},
	{"ADC CODEC_TEST", NULL, "AUD ADC1L"},
	{"ADC CODEC_TEST", NULL, "AUD ADC1R"},
	{"ADC LOOP", NULL, "AUD ADC1L"},
	{"ADC LOOP", NULL, "AUD ADC1R"},
	{"ADC", NULL, "AUD ADC1L"},
	{"ADC", NULL, "AUD ADC1R"},
	{"ADC AP23", NULL, "AUD ADC1L"},
	{"ADC AP23", NULL, "AUD ADC1R"},
	{"ADC1", NULL, "AUD ADC1L"},
	{"ADC1", NULL, "AUD ADC1R"},

	{"ADC", NULL, "CLK_ADC"},
	{"ADC1", NULL, "CLK_ADC"},
	{"ADC AP23", NULL, "CLK_ADC"},
	{"ADC DSP_C", NULL, "CLK_ADC"},
	{"ADC DSP_C_btsco_test", NULL, "CLK_ADC"},
	{"ADC DSP_C_fm_test", NULL, "CLK_ADC"},
	{"ADC Voice", NULL, "CLK_ADC"},
	{"Capture RECOGNISE", NULL, "CLK_ADC"},
	{"ADC Voip", NULL, "CLK_ADC"},
	{"ADC CODEC_TEST", NULL, "CLK_ADC"},
	{"ADC LOOP", NULL, "CLK_ADC"},

/* MIC */
	{"MICBIAS1", NULL, "BG"},
	{"MICBIAS2", NULL, "BG"},
	{"MICBIAS3", NULL, "BG"},
	{"MIC Pin", NULL, "MICBIAS1"},
	{"MIC2 Pin", NULL, "MICBIAS2"},
	{"MIC3 Pin", NULL, "MICBIAS3"},
	{"HPMIC Pin", NULL, "HEADMICBIAS"},
	{"ADC_1 Mixer", "MIC1PGA_1 Switch", "MIC Pin"},
	{"ADC_2 Mixer", "MIC2PGA_2 Switch", "MIC2 Pin"},
	{"ADC_3 Mixer", "MIC3PGA_3 Switch", "MIC3 Pin"},
	{"ADC_1 Mixer", "HPMICPGA_1 Switch", "HPMIC Pin"},
	{"ADC_2 Mixer", "HPMICPGA_2 Switch", "HPMIC Pin"},
	{"ADC_3 Mixer", "HPMICPGA_3 Switch", "HPMIC Pin"},

/* ADie loop */
	{"ADC-DAC Adie Loop", "switch", "ADC"},
	{"ADC1-DAC Adie Loop", "switch", "ADC1"},
	{"ADC-DAC Adie Loop post", NULL, "ADC-DAC Adie Loop"},
	{"ADC1-DAC Adie Loop post", NULL, "ADC1-DAC Adie Loop"},
	{"DAC", NULL, "ADC-DAC Adie Loop post"},
	{"DAC", NULL, "ADC1-DAC Adie Loop post"},

/* Ddie loop */
	{"ADC-DAC Digital Loop", "switch", "ADC"},
	{"ADC1-DAC Digital Loop", "switch", "ADC1"},
	{"ADC-DAC Digital Loop post", NULL, "ADC-DAC Digital Loop"},
	{"ADC1-DAC Digital Loop post", NULL, "ADC1-DAC Digital Loop"},
	{"DAC", NULL, "ADC-DAC Digital Loop post"},
	{"DAC", NULL, "ADC1-DAC Digital Loop post"},

	/* DMIC0 */
	{"DMIC Switch", NULL, "DMIC Pin"},
	{"Digital ADC In Sel", "DMIC", "DMIC Switch"},
	/* DMIC1 */
	{"DMIC1 Switch", NULL, "DMIC1 Pin"},
	{"Digital ADC1 In Sel", "DMIC", "DMIC1 Switch"},

	/* virt path */
	{"Virt Output", "Switch", "Digital DACL Switch"},
	{"Virt Output", "Switch", "Digital DACR Switch"},
	{"Virt Output Pin", NULL, "Virt Output"},
};

static int sprd_codec_info_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_info("chip_id 0x%x, ver_id 0x%x, hw_index %d\n",
			sprd_codec->ana_chip_id, sprd_codec->ana_chip_ver_id,
			AUDIO_CODEC_9620);

	ucontrol->value.integer.value[0] = AUDIO_CODEC_9620;
	return 0;
}

static int sprd_codec_inter_pa_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int invert = mc->invert;

	ucontrol->value.integer.value[0] = sprd_codec->inter_pa.value;

	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_inter_pa_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int mask = BIT(fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;

	sp_asoc_pr_info("Config inter PA 0x%lx\n",
			ucontrol->value.integer.value[0]);

	val = ucontrol->value.integer.value[0] & mask;
	if (invert)
		val = max - val;

	sprd_codec->inter_pa.value = val;

	return 0;
}

static int sprd_codec_dac_lrclk_sel_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	val = sprd_codec->lrclk_sel[LRCLK_SEL_DAC];
	ucontrol->value.enumerated.item[0] = !!val;

	sp_asoc_pr_dbg("%s %u\n", __func__,
		       ucontrol->value.enumerated.item[0]);

	return 0;
}

static int sprd_codec_dac_lrclk_sel_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s %d\n", __func__,
		       (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_DAC] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(DAC_LR_SEL) : 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s agdsp_access_enable error %d", __func__, ret);
		return ret;
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL), BIT(DAC_LR_SEL), val);
	agdsp_access_disable();

	return 0;
}

static int sprd_codec_adc_lrclk_sel_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	val = sprd_codec->lrclk_sel[LRCLK_SEL_ADC];
	ucontrol->value.enumerated.item[0] = !!val;

	sp_asoc_pr_dbg("%s %u\n", __func__,
		       ucontrol->value.enumerated.item[0]);

	return 0;
}

static int sprd_codec_adc_lrclk_sel_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	int ret;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s %d\n", __func__,
		       (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_ADC] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(ADC_LR_SEL) : 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s agdsp_access_enable error %d", __func__, ret);
		return ret;
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL),
			    BIT(ADC_LR_SEL), val);
	agdsp_access_disable();

	return 0;
}

static int sprd_codec_adc1_lrclk_sel_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	val = sprd_codec->lrclk_sel[LRCLK_SEL_ADC1];
	ucontrol->value.enumerated.item[0] = !!val;

	sp_asoc_pr_dbg("%s %u\n", __func__,
		       ucontrol->value.enumerated.item[0]);

	return 0;
}

static int sprd_codec_adc1_lrclk_sel_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	int ret;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s %d\n", __func__,
		       (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_ADC1] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(ADC1_LR_SEL) : 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s agdsp_access_enable error %d", __func__, ret);
		return ret;
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC1_I2S_CTL),
			    BIT(ADC1_LR_SEL), val);
	agdsp_access_disable();

	return 0;
}

static int sprd_codec_fixed_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->fixed_sample_rate[0];

	return 0;
}

static int sprd_codec_fixed_rate_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int i;
	u32 fixed_rate;

	fixed_rate = ucontrol->value.integer.value[0];

	sp_asoc_pr_info("%s fixed rate %u\n", __func__, fixed_rate);
	for (i = 0; i < CODEC_PATH_MAX; i++)
		sprd_codec->fixed_sample_rate[i] = fixed_rate;

	return 0;
}

static int sprd_codec_get_hp_mix_mode(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->hp_mix_mode;

	return 0;
}

#ifdef AUTO_DA_MODE_SWITCH
static struct snd_kcontrol *sprd_codec_find_ctrl(struct snd_soc_codec *codec,
						 char *name)
{
	struct snd_soc_card *card = codec ? codec->component.card : NULL;
	struct snd_ctl_elem_id id = {.iface = SNDRV_CTL_ELEM_IFACE_MIXER};

	if (!codec || !name)
		return NULL;
	memcpy(id.name, name, strlen(name));

	return snd_ctl_find_id(card->snd_card, &id);
}

static int sprd_codec_get_ctrl(struct snd_soc_codec *codec, char *name)
{
	struct snd_kcontrol *kctrl;
	int ret = 0;

	kctrl = sprd_codec_find_ctrl(codec, name);
	if (kctrl)
		ret = dapm_kcontrol_get_value(kctrl);

	return ret;
}

static int sprd_codec_da_mode_switch(struct snd_soc_codec *codec, bool on)
{
	struct snd_kcontrol *kctrl;
	struct snd_ctl_elem_value ucontrol = {
		.value.integer.value[0] = on,
	};

	kctrl = sprd_codec_find_ctrl(codec, "DA mode Switch");
	sp_asoc_pr_dbg("%s ctrl=%p, %s\n", __func__,
		       kctrl, on ? "on" : "off");
	if (!kctrl)
		return -EPERM;

	return snd_soc_dapm_put_volsw(kctrl, &ucontrol);
}
#endif

static int sprd_codec_set_hp_mix_mode(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->hp_mix_mode = ucontrol->value.integer.value[0];

	return 0;
}

static int sprd_codec_spk_dg_fall_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->spk_fall_dg;

	return 0;
}

static int sprd_codec_spk_dg_fall_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->spk_fall_dg = ucontrol->value.integer.value[0];
	return 0;
}

static int sprd_codec_das_input_mux_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = sprd_codec->das_input_mux;

	return 0;
}

static int sprd_codec_das_input_mux_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->das_input_mux = ucontrol->value.enumerated.item[0];
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_ANA_ET2),
			    BIT_RG_AUD_DAS_MIX_SEL(0x3),
			    BIT_RG_AUD_DAS_MIX_SEL(sprd_codec->das_input_mux));

	return 0;
}

static int sprd_codec_ivsence_dmic_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->ivsence_dmic_type;

	return 0;
}

static int sprd_codec_ivsence_dmic_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= e->items || val < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_info("%s using %s, val %d\n", __func__, e->texts[val], val);
	sprd_codec->ivsence_dmic_type = val;

	return 0;
}

static int sprd_codec_daldo_bypass_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->daldo_bypass;

	return 0;
}

static int sprd_codec_daldo_bypass_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->daldo_bypass = !!ucontrol->value.integer.value[0];
	sp_asoc_pr_info("%s daldo_bypass %d\n", __func__,
			sprd_codec->daldo_bypass);

	return 0;
}

static int sprd_codec_clk_26m_in_sel_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->clk_26m_in_sel;

	return 0;
}

static int sprd_codec_clk_26m_in_sel_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->clk_26m_in_sel =
		(enum CLK_26M_IN_TYPE)ucontrol->value.integer.value[0];
	sp_asoc_pr_info("clk 26m in sel %s\n",
		sprd_codec->clk_26m_in_sel ? "AUD_PLL_IN" : "AUD_26M_IN");

	return 0;
}

static int sprd_codec_drv_soft_t_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->drv_soft_t;

	return 0;
}

static int sprd_codec_drv_soft_t_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] > mc->max ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->drv_soft_t = ucontrol->value.integer.value[0];
	sp_asoc_pr_info("drv_soft_t %d\n", sprd_codec->drv_soft_t);

	return 0;
}

static int sprd_codec_daaol_buf_dccal_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->daaol_buf_dccal;

	return 0;
}

static int sprd_codec_daaol_buf_dccal_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->daaol_buf_dccal =
		(enum DA_BUF_CAL_MODE_TYPE)ucontrol->value.integer.value[0];
	sp_asoc_pr_info("daaol_buf_dccal %s\n",
			da_buf_cal_mode_txt[sprd_codec->daaol_buf_dccal]);

	return 0;
}

static int sprd_codec_daaor_buf_dccal_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->daaor_buf_dccal;

	return 0;
}

static int sprd_codec_daaor_buf_dccal_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->daaor_buf_dccal =
		(enum DA_BUF_CAL_MODE_TYPE)ucontrol->value.integer.value[0];
	sp_asoc_pr_info("daaor_buf_dccal %s\n",
			da_buf_cal_mode_txt[sprd_codec->daaor_buf_dccal]);

	return 0;
}

static int sprd_codec_dahp_os_inv_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->dahp_os_inv;

	return 0;
}

static int sprd_codec_dahp_os_inv_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->dahp_os_inv =
		(enum INVERT_MODE_TYPE)ucontrol->value.integer.value[0];
	sp_asoc_pr_info("dahp_os_inv %s\n",
			sprd_codec->dahp_os_inv ? "invert" : "normal");

	return 0;
}

static int sprd_codec_dahp_os_d_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->dahp_os_d;

	return 0;
}

static int sprd_codec_dahp_os_d_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] > mc->max ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->dahp_os_d = ucontrol->value.integer.value[0];
	sp_asoc_pr_info("dahp_os_d %d\n", sprd_codec->dahp_os_d);

	return 0;
}

static int sprd_codec_dahpl_buf_dccal_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->dahpl_buf_dccal;

	return 0;
}

static int sprd_codec_dahpl_buf_dccal_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->dahpl_buf_dccal =
		(enum DA_BUF_CAL_MODE_TYPE)ucontrol->value.integer.value[0];
	sp_asoc_pr_info("dahpl_buf_dccal %s\n",
			da_buf_cal_mode_txt[sprd_codec->dahpl_buf_dccal]);

	return 0;
}

static int sprd_codec_dahpr_buf_dccal_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->dahpr_buf_dccal;

	return 0;
}

static int sprd_codec_dahpr_buf_dccal_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->dahpr_buf_dccal =
		(enum DA_BUF_CAL_MODE_TYPE)ucontrol->value.integer.value[0];
	sp_asoc_pr_info("dahpr_buf_dccal %s\n",
			da_buf_cal_mode_txt[sprd_codec->dahpr_buf_dccal]);

	return 0;
}

static int sprd_codec_rcv_pop_ramp_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->rcv_pop_ramp;

	return 0;
}

static int sprd_codec_rcv_pop_ramp_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] > mc->max ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->rcv_pop_ramp = ucontrol->value.integer.value[0];
	sp_asoc_pr_info("rcv_pop_ramp %d\n", sprd_codec->rcv_pop_ramp);

	return 0;
}

static int sprd_codec_get_sdm_l(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->sdm_l;
	sp_asoc_pr_info("ramp read SDM_L %d ( 0x%x)\n",
			sprd_codec->sdm_l, sprd_codec->sdm_l);

	return 0;
}

static int sprd_codec_set_sdm_l(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->sdm_l = ucontrol->value.integer.value[0];
	sp_asoc_pr_info("ramp set SDM_L %d ( 0x%x)\n",
			sprd_codec->sdm_l, sprd_codec->sdm_l);

	return 0;
}

static int sprd_codec_get_sdm_h(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->sdm_h;
	sp_asoc_pr_info("ramp read SDM_H %d ( 0x%x)\n",
			sprd_codec->sdm_h, sprd_codec->sdm_h);

	return 0;
}

static int sprd_codec_set_sdm_h(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->sdm_h = ucontrol->value.integer.value[0];
	sp_asoc_pr_info("ramp set SDM_H %d ( 0x%x)\n",
			sprd_codec->sdm_h, sprd_codec->sdm_h);

	return 0;
}

static int sprd_codec_hp_chn_type_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->hp_chn_type;

	return 0;
}

static int sprd_codec_hp_chn_type_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("%s index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sprd_codec->hp_chn_type =
		(enum HP_CHANNEL_TYPE)ucontrol->value.integer.value[0];
	sp_asoc_pr_info("hp type %s\n",
			hp_chn_type_txt[sprd_codec->hp_chn_type]);

	return 0;
}

static const struct snd_kcontrol_new sprd_codec_snd_controls[] = {
	SOC_ENUM_EXT("Aud Codec Info", codec_info_enum,
		     sprd_codec_info_get, NULL),
	SOC_SINGLE_EXT("Inter PA Config", 0, 0, INT_MAX, 0,
		       sprd_codec_inter_pa_get, sprd_codec_inter_pa_put),
	SOC_ENUM_EXT("DAC LRCLK Select", lrclk_sel_enum,
		     sprd_codec_dac_lrclk_sel_get,
		     sprd_codec_dac_lrclk_sel_put),
	SOC_ENUM_EXT("ADC LRCLK Select", lrclk_sel_enum,
		     sprd_codec_adc_lrclk_sel_get,
		     sprd_codec_adc_lrclk_sel_put),
	SOC_ENUM_EXT("ADC1 LRCLK Select", lrclk_sel_enum,
		     sprd_codec_adc1_lrclk_sel_get,
		     sprd_codec_adc1_lrclk_sel_put),
	SOC_SINGLE_EXT("HP mix mode", 0, 0, INT_MAX, 0,
		       sprd_codec_get_hp_mix_mode, sprd_codec_set_hp_mix_mode),
	SOC_SINGLE_EXT("SPK DG fall", 0, 0, INT_MAX, 0,
		       sprd_codec_spk_dg_fall_get, sprd_codec_spk_dg_fall_set),

	SOC_ENUM_EXT("DAS Input Mux", das_input_mux_enum,
		     sprd_codec_das_input_mux_get,
		     sprd_codec_das_input_mux_put),
	SOC_SINGLE_EXT("TEST_FIXED_RATE", 0, 0, INT_MAX, 0,
		       sprd_codec_fixed_rate_get, sprd_codec_fixed_rate_put),
	SOC_SINGLE_EXT("Codec Digital Access Disable", SND_SOC_NOPM, 0, 1, 0,
		       dig_access_disable_get, dig_access_disable_put),
	SOC_ENUM_EXT("IVSENCE_DMIC_SEL", ivsence_dmic_sel_enum,
		     sprd_codec_ivsence_dmic_get,
		     sprd_codec_ivsence_dmic_put),
	SOC_SINGLE_EXT("DALDO BYPASS", SND_SOC_NOPM, 0, 1, 0,
		       sprd_codec_daldo_bypass_get,
		       sprd_codec_daldo_bypass_put),
	/* AUD_26M_IN in default */
	SOC_ENUM_EXT("CLK 26M IN SEL", clk_26m_in_sel_enum,
		     sprd_codec_clk_26m_in_sel_get,
		     sprd_codec_clk_26m_in_sel_put),
	SOC_SINGLE_EXT("DRV SOFT T", SND_SOC_NOPM, 0, 7, 0,
		       sprd_codec_drv_soft_t_get, sprd_codec_drv_soft_t_put),
	/* hw_mode in default */
	SOC_ENUM_EXT("DAAOL BUF DCCAL", da_buf_dccal_mode_enum,
		     sprd_codec_daaol_buf_dccal_get,
		     sprd_codec_daaol_buf_dccal_put),
	SOC_ENUM_EXT("DAAOR BUF DCCAL", da_buf_dccal_mode_enum,
		     sprd_codec_daaor_buf_dccal_get,
		     sprd_codec_daaor_buf_dccal_put),
	SOC_ENUM_EXT("DAHP OS INV", lrclk_sel_enum,
		     sprd_codec_dahp_os_inv_get, sprd_codec_dahp_os_inv_put),
	SOC_SINGLE_EXT("DAHP OS D", SND_SOC_NOPM, 0, 7, 0,
		       sprd_codec_dahp_os_d_get, sprd_codec_dahp_os_d_put),
	SOC_ENUM_EXT("DAHPL BUF DCCAL", da_buf_dccal_mode_enum,
		     sprd_codec_dahpl_buf_dccal_get,
		     sprd_codec_dahpl_buf_dccal_put),
	SOC_ENUM_EXT("DAHPR BUF DCCAL", da_buf_dccal_mode_enum,
		     sprd_codec_dahpr_buf_dccal_get,
		     sprd_codec_dahpr_buf_dccal_put),
	SOC_SINGLE_EXT("RCV POP RAMP", SND_SOC_NOPM, 0, 1, 0,
		       sprd_codec_rcv_pop_ramp_get,
		       sprd_codec_rcv_pop_ramp_put),
	SOC_SINGLE_EXT("SDM_L SET", 0, 0, INT_MAX, 0,
		       sprd_codec_get_sdm_l, sprd_codec_set_sdm_l),
	SOC_SINGLE_EXT("SDM_H SET", 0, 0, INT_MAX, 0,
		       sprd_codec_get_sdm_h, sprd_codec_set_sdm_h),
	SOC_ENUM_EXT("HP CHN TYPE", hp_chn_type_enum,
		     sprd_codec_hp_chn_type_get,
		     sprd_codec_hp_chn_type_put),
};

static unsigned int sprd_codec_read(struct snd_soc_codec *codec,
				    unsigned int reg)
{
	int ret = 0;

	/*
	 * Because snd_soc_update_bits reg is 16 bits short type,
	   so muse do following convert
	 */
	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		return arch_audio_codec_read(reg);
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;

		ret = agdsp_access_enable();
		if (ret) {
			pr_err("%s, agdsp_access_enable failed\n", __func__);
			return ret;
		}
		codec_digital_reg_enable(codec);
		ret = readl_relaxed((void __iomem *)(reg -
			CODEC_DP_BASE + sprd_codec_dp_base));
		codec_digital_reg_disable(codec);
		agdsp_access_disable();

		return ret;
	}

	sp_asoc_pr_info("read, register is not codec's reg 0x%x\n",
			reg);

	return 0;
}

static int sprd_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			    unsigned int val)
{
	int ret = 0;

	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		sp_asoc_pr_reg("A[0x%04x] R:[0x%08x]\n",
			       (reg - CODEC_AP_BASE) & 0xFFFF,
			       arch_audio_codec_read(reg));
		ret = arch_audio_codec_write(reg, val);
		sp_asoc_pr_reg("A[0x%04x] W:[0x%08x] R:[0x%08x]\n",
			       (reg - CODEC_AP_BASE) & 0xFFFF,
			       val, arch_audio_codec_read(reg));
		return ret;
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;

		ret = agdsp_access_enable();
		if (ret) {
			pr_err("%s, agdsp_access_enable failed\n", __func__);
			return ret;
		}
		codec_digital_reg_enable(codec);
		sp_asoc_pr_reg("D[0x%04x] R:[0x%08x]\n",
			       (reg - CODEC_DP_BASE) & 0xFFFF,
			       readl_relaxed((void __iomem *)(reg -
				CODEC_DP_BASE + sprd_codec_dp_base)));

		writel_relaxed(val, (void __iomem *)(reg -
			CODEC_DP_BASE + sprd_codec_dp_base));
		sp_asoc_pr_reg("D[0x%04x] W:[0x%08x] R:[0x%08x]\n",
			       (reg - CODEC_DP_BASE) & 0xFFFF, val,
			       readl_relaxed((void __iomem *)(reg -
				CODEC_DP_BASE + sprd_codec_dp_base)));
		codec_digital_reg_disable(codec);
		agdsp_access_disable();

		return ret;
	}

	sp_asoc_pr_info("write, register is not codec's reg 0x%x\n", reg);

	return ret;
}

static int sprd_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	int mask = 0xf, shift = 0;
	struct snd_soc_codec *codec = dai->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u32 *fixed_rate = sprd_codec->fixed_sample_rate;
	u32 rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sprd_codec->da_sample_val = fixed_rate[CODEC_PATH_DA] ?
			fixed_rate[CODEC_PATH_DA] : rate;
		sprd_codec_set_sample_rate(codec,
			sprd_codec->da_sample_val, mask, shift);
		sp_asoc_pr_info("Playback rate is [%u], ivsence_dmic_type %d\n",
				sprd_codec->da_sample_val,
				sprd_codec->ivsence_dmic_type);

		switch (sprd_codec->ivsence_dmic_type) {
		case DMIC0:
			sprd_codec->ad_sample_val =
				fixed_rate[CODEC_PATH_AD] ?
				fixed_rate[CODEC_PATH_AD] : rate;
			sprd_codec_set_ad_sample_rate(codec,
				sprd_codec->ad_sample_val, mask, shift);
			break;
		case DMIC1:
			sprd_codec->ad1_sample_val =
				fixed_rate[CODEC_PATH_AD1] ?
				fixed_rate[CODEC_PATH_AD1] : rate;
			sprd_codec_set_ad_sample_rate(codec,
				sprd_codec->ad1_sample_val,
				ADC1_SRC_N_MASK, ADC1_SRC_N);
			break;
		case DMIC0_1:
		case NONE:
		default:
			sp_asoc_pr_dbg("%s to do nothing\n", __func__);
			}
	} else {
		sprd_codec->ad_sample_val = fixed_rate[CODEC_PATH_AD] ?
			fixed_rate[CODEC_PATH_AD] : rate;
		sprd_codec_set_ad_sample_rate(codec,
			sprd_codec->ad_sample_val, mask, shift);
		sp_asoc_pr_info("Capture rate is [%u]\n",
				sprd_codec->ad_sample_val);

		sprd_codec->ad1_sample_val = fixed_rate[CODEC_PATH_AD1]
			? fixed_rate[CODEC_PATH_AD1] : rate;
		sprd_codec_set_ad_sample_rate(codec,
					      sprd_codec->ad1_sample_val,
					      ADC1_SRC_N_MASK, ADC1_SRC_N);
		sp_asoc_pr_info("Capture(ADC1) rate is [%u]\n",
				sprd_codec->ad1_sample_val);
	}

	return 0;
}

static int sprd_codec_pcm_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	return 0;
}

static int sprd_codec_pcm_hw_startup(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct sprd_codec_priv *sprd_codec =
		snd_soc_codec_get_drvdata(dai->codec);

	/* notify only once */
	if (sprd_codec->startup_cnt == 0)
		headset_set_audio_state(true);
	sprd_codec->startup_cnt++;
	sp_asoc_pr_dbg("%s, startup_cnt=%d\n",
		       __func__, sprd_codec->startup_cnt);
	return 0;
}

static void sprd_codec_pcm_hw_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct sprd_codec_priv *sprd_codec =
		snd_soc_codec_get_drvdata(dai->codec);

	if (sprd_codec->startup_cnt > 0)
		sprd_codec->startup_cnt--;
	if (sprd_codec->startup_cnt == 0)
		headset_set_audio_state(false);
	sp_asoc_pr_dbg("%s, startup_cnt=%d\n",
		       __func__, sprd_codec->startup_cnt);
}

static irqreturn_t sprd_codec_dp_irq(int irq, void *dev_id)
{
	int mask;
	struct sprd_codec_priv *sprd_codec = dev_id;
	struct snd_soc_codec *codec = sprd_codec->codec;

	mask = snd_soc_read(codec, AUD_AUD_STS0);
	sp_asoc_pr_dbg("dac mute irq mask = 0x%x\n", mask);
	if (BIT(DAC_MUTE_D_MASK) & mask) {
		mask = BIT(DAC_MUTE_D);
		complete(&sprd_codec->completion_dac_mute);
	}
	if (BIT(DAC_MUTE_U_MASK) & mask)
		mask = BIT(DAC_MUTE_U);
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_EN), mask, 0);

	return IRQ_HANDLED;
}

static struct snd_soc_dai_ops sprd_codec_dai_ops = {
	.hw_params = sprd_codec_pcm_hw_params,
	.hw_free = sprd_codec_pcm_hw_free,
	.startup = sprd_codec_pcm_hw_startup,
	.shutdown = sprd_codec_pcm_hw_shutdown,
};

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
/* procfs interfaces for factory test: check if audio PA P/N is shorted. */
static void pa_short_stat_proc_read(struct snd_info_entry *entry,
				    struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;

	snd_iprintf(buffer, "CP short stat=%d\nPA short stat=%d\n",
		    sprd_codec->cp_short_stat, sprd_codec->pa_short_stat);
}

static void sprd_codec_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;
	struct snd_soc_codec *codec = sprd_codec->codec;
	int reg, ret;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s agdsp_access_enable failed\n", __func__);
		return;
	}
	snd_iprintf(buffer, "%s digital part\n", codec->component.name);
	for (reg = SPRD_CODEC_DP_BASE; reg < SPRD_CODEC_DP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - SPRD_CODEC_DP_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		);
	}
	agdsp_access_disable();

	snd_iprintf(buffer, "%s analog part\n",
		    codec->component.name);
	for (reg = SPRD_CODEC_AP_BASE;
			reg < SPRD_CODEC_AP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - SPRD_CODEC_AP_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		);
	}

	/* For AUDIF registers
	 * 0x5c0 = 0x700 - 0x140;
	 */
	snd_iprintf(buffer, "%s audif\n", codec->component.name);
	for (reg = CTL_BASE_AUD_TOPA_RF; reg < 0x50 + CTL_BASE_AUD_TOPA_RF;
			reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - CTL_BASE_AUD_TOPA_RF)
			    , arch_audio_codec_read(reg + 0x00)
			    , arch_audio_codec_read(reg + 0x04)
			    , arch_audio_codec_read(reg + 0x08)
			    , arch_audio_codec_read(reg + 0x0C)
		);
	}
}

#define REG_PAIR_NUM 30
static void aud_glb_reg_read(struct snd_info_entry *entry,
			     struct snd_info_buffer *buffer)
{
	int i = 0, j = 0;
	struct glb_reg_dump *aud_glb_reg = NULL;
	struct glb_reg_dump *reg_p = NULL;

	aud_glb_reg = devm_kzalloc(entry->card->dev,
				   sizeof(struct glb_reg_dump) * REG_PAIR_NUM,
				   GFP_KERNEL);
	if (aud_glb_reg == NULL) {
		sp_asoc_pr_info("%s not enough memory error\n", __func__);
		return;
	}
	reg_p = aud_glb_reg;
	AUDIO_GLB_REG_DUMP_LIST(reg_p);

	snd_iprintf(buffer, "audio global register dump\n");
	for (i = 0; i < REG_PAIR_NUM; i++) {
		for (j = 0; j < aud_glb_reg[i].count; j++) {
			if (aud_glb_reg[i].func)
				goto free;
			if (j == 0)
				snd_iprintf(buffer, "%30s: 0x%08lx 0x%08x\n",
					    aud_glb_reg[i].reg_name,
					    aud_glb_reg[i].reg +
					    sizeof(aud_glb_reg[i].reg) *
					    j, aud_glb_reg[i].func(
					(void *)(aud_glb_reg[i].reg +
					sizeof(aud_glb_reg[i].reg) * j))
				);
			else
				snd_iprintf(buffer, "%27s+%2d: %#08lx %#08x\n",
					    aud_glb_reg[i].reg_name, j,
					    aud_glb_reg[i].reg +
					    sizeof(aud_glb_reg[i].reg) *
					    j, aud_glb_reg[i].func(
					(void *)(aud_glb_reg[i].reg +
					sizeof(aud_glb_reg[i].reg) * j))
				);
		}
	}

free:
	devm_kfree(entry->card->dev, aud_glb_reg);
}

static void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
	struct snd_info_entry *entry;
	struct snd_soc_codec *codec = sprd_codec->codec;
	struct snd_card *card = codec->component.card->snd_card;

	if (!snd_card_proc_new(card, "sprd-codec", &entry))
		snd_info_set_text_ops(entry, sprd_codec, sprd_codec_proc_read);
	if (!snd_card_proc_new(card, "aud-glb", &entry))
		snd_info_set_text_ops(entry, sprd_codec, aud_glb_reg_read);

	if (!snd_card_proc_new(card, "short-check-stat", &entry))
		snd_info_set_text_ops(entry, sprd_codec,
				      pa_short_stat_proc_read);
}

/* !CONFIG_PROC_FS */
#else
static inline void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
}
#endif

#define SPRD_CODEC_PCM_RATES \
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_11025 | \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_22050 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_96000)

#define SPRD_CODEC_PCM_AD_RATES \
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_32000 | \
	 UNSUPPORTED_AD_RATE | \
	 SNDRV_PCM_RATE_48000)

#define SPRD_CODEC_PCM_FATMATS (SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE)

/* PCM Playing and Recording default in full duplex mode */
static struct snd_soc_dai_driver sprd_codec_dai[] = {
/* 0: NORMAL_AP01 */
	{
		.name = "sprd-codec-normal-ap01",
		.playback = {
			.stream_name = "Normal-Playback-AP01",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Normal-Capture-AP01",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 1: NORMAL_AP23 */
	{
		.name = "sprd-codec-normal-ap23",
		.playback = {
			.stream_name = "Normal-Playback-AP23",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Normal-Capture-AP23",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 2: CAPTURE_DSP */
	{
		.name = "sprd-codec-capture-dsp",
		.capture = {
			.stream_name = "Capture-DSP",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 3: FAST_P */
	{
		.name = "sprd-codec-fast-playback",
		.playback = {
			.stream_name = "Fast-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 4: OFFLOAD */
	{
		.name = "sprd-codec-offload-playback",
		.playback = {
			.stream_name = "Offload-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 5: VOICE */
	{
		.name = "sprd-codec-voice",
		.playback = {
			.stream_name = "Voice-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Voice-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 6: VOIP */
	{
		.name = "sprd-codec-voip",
		.playback = {
			.stream_name = "Voip-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Voip-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 7: FM */
	{
		.name = "sprd-codec-fm",
		.playback = {
			.stream_name = "Fm-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 8 loopback(dsp voice) */
	{
		.name = "sprd-codec-loop",
		.playback = {
			.stream_name = "LOOP-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "LOOP-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 9: FM dsp*/
	{
		.name = "sprd-codec-fm-dsp",
		.playback = {
			.stream_name = "Fm-DSP-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 10 */
	{
		.id = SPRD_CODEC_IIS0_ID,
		.name = "sprd-codec-ad1",
		.capture = {
			.stream_name = "Ext-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 11 */
	{
		.id = SPRD_CODEC_IIS0_ID,
		.name = "sprd-codec-ad1-voice",
		.capture = {
			.stream_name = "Ext-Voice-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 12: CAPTURE_DSP_fm test haps, remove it if not haps */
	{
		.name = "test_fm_codec_replace",
		.capture = {
			.stream_name = "Capture-DSP-FM-test",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 13: CAPTURE_DSP_btsco test haps, remove it if not haps */
	{
		.name = "test_btsco_codec_replace",
		.capture = {
			.stream_name = "Capture-DSP-BTSCO-test",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 14: codec test */
	{
		.name = "sprd-codec-test",
		.playback = {
			.stream_name = "CODEC_TEST-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "CODEC_TEST-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},

	/* 15: CAPTURE_DSP_RECOGNISE */
	{
		.name = "sprd-codec-capture-dsp-recognise",
		.capture = {
			.stream_name = "Capture-DSP-RECOGNISE",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
};

static void codec_reconfig_dai_rate(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct sprd_card_data *mdata = snd_soc_card_get_drvdata(card);
	unsigned int replace_adc_rate =
		mdata ? mdata->codec_replace_adc_rate : 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int *fixed_rates = sprd_codec->fixed_sample_rate;
	unsigned int unsupported_rates = UNSUPPORTED_AD_RATE;
	struct snd_soc_dai *dai;

	if (replace_adc_rate &&
	    (fixed_rates[CODEC_PATH_AD] || fixed_rates[CODEC_PATH_AD1]))
		dev_warn(codec->dev, "%s, both replacingrate and fixed rate are set for adc\n",
			 __func__);

	/* VBC SRC supported input rate: 32000, 44100, 48000. */
	if (replace_adc_rate == 32000 || replace_adc_rate == 48000) {
		sprd_codec->replace_rate = replace_adc_rate;
		dev_info(codec->dev, "%s, use rate(%u) for the unsupported rate capture\n",
			 __func__, replace_adc_rate);
		return;
	}

	/*
	 * If fixed_rates is provided, tell user that this codec supports
	 * all kinds of rates.
	 */
	list_for_each_entry(dai, &codec->component.dai_list, list) {
		if (fixed_rates[CODEC_PATH_DA])
			dai->driver->playback.rates = SNDRV_PCM_RATE_CONTINUOUS;
		if ((dai->driver->id == SPRD_CODEC_IIS0_ID &&
		     fixed_rates[CODEC_PATH_AD]) ||
		    (dai->driver->id != SPRD_CODEC_IIS0_ID &&
		     fixed_rates[CODEC_PATH_AD1]))
			dai->driver->capture.rates = SNDRV_PCM_RATE_CONTINUOUS;
		else
			dai->driver->capture.rates &= ~unsupported_rates;
	}
}

static int sprd_codec_soc_probe(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	if (!sprd_codec) {
		pr_err("%s sprd_codec NULL error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s\n", __func__);

	codec_reconfig_dai_rate(codec);

	dapm->idle_bias_off = 1;

	sprd_codec->codec = codec;

	sprd_codec_proc_init(sprd_codec);

	snd_soc_dapm_ignore_suspend(dapm, "Offload-Playback");
	snd_soc_dapm_ignore_suspend(dapm, "Fm-Playback");
	snd_soc_dapm_ignore_suspend(dapm, "Voice-Playback");
	snd_soc_dapm_ignore_suspend(dapm, "Voice-Capture");
	snd_soc_dapm_ignore_suspend(dapm, "Capture-DSP-RECOGNISE");

	/*
	 * Even without headset driver, codec could work well.
	 * So, igore the return status here.
	 */
	ret = sprd_headset_soc_probe(codec);
	if (ret == -EPROBE_DEFER) {
		sp_asoc_pr_info("The headset is not ready now\n");
		return ret;
	}

	return 0;
}

/* power down chip */
static int sprd_codec_soc_remove(struct snd_soc_codec *codec)
{
	sprd_headset_remove();

	return 0;
}

static int sprd_codec_power_regulator_init(struct sprd_codec_priv *sprd_codec,
					   struct device *dev)
{
	int ret;

	sprd_codec_power_get(dev, &sprd_codec->head_mic, "HEADMICBIAS");
	sprd_codec_power_get(dev, &sprd_codec->vb, "VB");
	ret = regulator_enable(sprd_codec->vb);
	if (!ret) {
		regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_STANDBY);
		regulator_disable(sprd_codec->vb);
	}
	return 0;
}

static void sprd_codec_power_regulator_exit(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec_power_put(&sprd_codec->main_mic);
	sprd_codec_power_put(&sprd_codec->head_mic);
	sprd_codec_power_put(&sprd_codec->vb);
}

static struct snd_soc_codec_driver soc_codec_dev_sprd_codec = {
	.probe = sprd_codec_soc_probe,
	.remove = sprd_codec_soc_remove,
	.read = sprd_codec_read,
	.write = sprd_codec_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.component_driver = {
		.dapm_widgets = sprd_codec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
		.dapm_routes = sprd_codec_intercon,
		.num_dapm_routes = ARRAY_SIZE(sprd_codec_intercon),
		.controls = sprd_codec_snd_controls,
		.num_controls = ARRAY_SIZE(sprd_codec_snd_controls),
	}
};

enum {
	SOC_TYPE_WITH_AGCP,
	SOC_TYPE_WITHOUT_AGCP
};

static int sprd_codec_get_soc_type(struct device_node *np)
{
	int ret;
	const char *cmptbl;

	if (!np) {
		pr_err("%s np is NULL error\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_string(np, "compatible", &cmptbl);
	if (unlikely(ret)) {
		pr_err("%s node '%s' has no compatible prop?\n",
		       __func__, np->name);
		return -ENODEV;
	}

	if (strstr(cmptbl, "agcp"))
		return SOC_TYPE_WITH_AGCP;

	return SOC_TYPE_WITHOUT_AGCP;
}

static int sprd_codec_dig_probe(struct platform_device *pdev)
{
	int ret = 0;
	int soc_type;
	u32 val = 0;
	struct resource *res;
	struct regmap *agcp_ahb_gpr;
	struct regmap *aon_apb_gpr;
	struct regmap *anlg_phy_gpr;
	struct device_node *np = pdev->dev.of_node;
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);

	if (!np) {
		pr_err("%s np NULL error\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!sprd_codec) {
		pr_err("%s sprd_codec_priv NULL error\n", __func__);
		return -ENOMEM;
	}

	soc_type = sprd_codec_get_soc_type(np);
	if (soc_type == SOC_TYPE_WITH_AGCP) {
		/* Prepare for registers accessing. */
		agcp_ahb_gpr = syscon_regmap_lookup_by_phandle(
			np, "sprd,syscon-agcp-ahb");
		if (IS_ERR(agcp_ahb_gpr)) {
			pr_err("%s Get the codec aon apb syscon failed %ld\n",
			       __func__, PTR_ERR(agcp_ahb_gpr));
			agcp_ahb_gpr = NULL;
			return -EPROBE_DEFER;
		}
		arch_audio_set_agcp_ahb_gpr(agcp_ahb_gpr);
	} else if (soc_type == SOC_TYPE_WITHOUT_AGCP) {
		aon_apb_gpr = syscon_regmap_lookup_by_phandle(
			np, "sprd,syscon-aon-apb");
		if (IS_ERR(aon_apb_gpr)) {
			pr_err("%s Get the codec aon apb syscon failed %ld\n",
			       __func__, PTR_ERR(aon_apb_gpr));
			aon_apb_gpr = NULL;
			return -EPROBE_DEFER;
		}
		arch_audio_set_aon_apb_gpr(aon_apb_gpr);

		arch_audio_codec_switch2ap();
		val = platform_get_irq(pdev, 0);
		if (val > 0) {
			sprd_codec->dp_irq = val;
			sp_asoc_pr_dbg("Set DP IRQ is %u\n", val);
		} else {
			pr_err("Must give me the DP IRQ, error\n");
			return -EINVAL;
		}
		ret = devm_request_irq(&pdev->dev, sprd_codec->dp_irq,
				       sprd_codec_dp_irq, 0,
				       "sprd_codec_dp", sprd_codec);
		if (ret) {
			pr_err("Request irq dp failed\n");
			return ret;
		}
		/* initial value for FM route */
		sprd_codec->da_sample_val = 44100;
		if (!of_property_read_u32(np, "sprd,def_da_fs", &val)) {
			sprd_codec->da_sample_val = val;
			sp_asoc_pr_dbg("Change DA default fs to %u\n", val);
		}
	} else {
		pr_err("%s error, unknown soc type %d", __func__, soc_type);
		return -EINVAL;
	}
	anlg_phy_gpr = syscon_regmap_lookup_by_phandle(np,
						"sprd,anlg-phy-g-syscon");
	if (IS_ERR(anlg_phy_gpr)) {
		dev_warn(&pdev->dev, "Get the sprd,anlg-phy-g-syscon failed %ld\n",
			 PTR_ERR(anlg_phy_gpr));
		anlg_phy_gpr = NULL;
	} else {
		arch_audio_set_anlg_phy_g(anlg_phy_gpr);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res != NULL) {
		sprd_codec_dp_base = (unsigned long)
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR_VALUE(sprd_codec_dp_base)) {
			pr_err("error, cannot create iomap address for codec DP\n");
			return -EINVAL;
		}
	} else {
		pr_err("error, Must give me the codec DP reg address\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_codec_dt_parse_mach(struct platform_device *pdev)
{
	int ret = 0, i;
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;

	if (!np || !sprd_codec) {
		pr_err("%s error, np(%p) or sprd_codec(%p) is NULL\n",
		       __func__, np, sprd_codec);
		return 0;
	}

	/* Parsing codec properties located in machine. */
	ret = of_property_read_u32_array(np, "fixed-sample-rate",
					 sprd_codec->fixed_sample_rate,
					 CODEC_PATH_MAX);
	if (ret) {
		if (ret != -EINVAL)
			pr_warn("%s parsing 'fixed-sample-rate' failed\n",
				__func__);
		for (i = 0; i < CODEC_PATH_MAX; i++)
			sprd_codec->fixed_sample_rate[i] = 0;
	}
	pr_debug("%s fixed sample rate of codec: %u, %u, %u\n", __func__,
		 sprd_codec->fixed_sample_rate[CODEC_PATH_DA],
		 sprd_codec->fixed_sample_rate[CODEC_PATH_AD],
		 sprd_codec->fixed_sample_rate[CODEC_PATH_AD1]);

	return 0;
}

static int sprd_codec_ana_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 val;
	struct device_node *np = pdev->dev.of_node;
	struct regmap *adi_rgmp;
	struct sprd_headset_global_vars glb_vars;
	struct sprd_codec_priv *sprd_codec;

	if (!np) {
		pr_err("%s error, there must be a analog node\n", __func__);
		return -EPROBE_DEFER;
	}

	sprd_codec = platform_get_drvdata(pdev);
	if (!sprd_codec) {
		pr_err("%s error, sprd_codec_priv is NULL\n", __func__);
		return -ENOMEM;
	}

	/* Prepare for registers accessing. */
	adi_rgmp = dev_get_regmap(pdev->dev.parent, NULL);
	if (!adi_rgmp) {
		pr_err("%s error, spi device is not ready yet\n", __func__);
		return -EPROBE_DEFER;
	}
	ret = of_property_read_u32(np, "reg", &val);
	if (ret) {
		pr_err("%s error, no property of 'reg'\n", __func__);
		return -ENXIO;
	}
	pr_err("%s adi_rgmp %p, val 0x%x\n", __func__, adi_rgmp, val);
	arch_audio_codec_set_regmap(adi_rgmp);
	arch_audio_codec_set_reg_offset((unsigned long)val);
	/* Set global register accessing vars for headset. */
	glb_vars.regmap = adi_rgmp;
	glb_vars.codec_reg_offset = val;
	/* yintang: marked for compiling */
	sprd_headset_set_global_variables(&glb_vars);

	/* Parsing configurations varying as machine. */
	ret = sprd_codec_dt_parse_mach(pdev);
	if (ret)
		return ret;

	return 0;
}

/* This @pdev is the one corresponding to analog part dt node. */
static int sprd_codec_probe(struct platform_device *pdev)
{
	struct sprd_codec_priv *sprd_codec;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dig_np = NULL;
	struct platform_device *dig_pdev = NULL;
	u32 set_offset;
	u32 clr_offset;
	u32 lrdat_sel;
	struct regmap *pmu_apb_gpr;
	int args_count;
	unsigned int args[2];
	int ret;

	sp_asoc_pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s Only OF is supported yet\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	sprd_codec = devm_kzalloc(&pdev->dev,
				  sizeof(struct sprd_codec_priv), GFP_KERNEL);
	if (sprd_codec == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	platform_set_drvdata(pdev, sprd_codec);

	sprd_codec->aon_top_apb = syscon_regmap_lookup_by_phandle(np,
		"sprd,syscon-aon-apb");
	if (IS_ERR(sprd_codec->aon_top_apb)) {
		pr_err("%s get aon top apb syscon failed\n", __func__);
		sprd_codec->aon_top_apb = NULL;
		ret = -EPROBE_DEFER;
		goto error;
	}
	args_count = syscon_get_args_by_name(np, "clk26m_sinout_pmic_en",
		sizeof(args), args);
	if (args_count == ARRAY_SIZE(args)) {
		sprd_codec->clk26m_sinout_pmic_reg = args[0];
		sprd_codec->clk26m_sinout_pmic_mask = args[1];
		pr_info("clk26m_sinout_pmic reg 0x%x, mask 0x%x\n",
			args[0], args[1]);
	}
	ret = sprd_codec_clk26m_sinout(sprd_codec, true);
	if (ret)
		pr_err("%s set clk26m_sinout_pmic fail\n");

	/* Probe for analog part(in PMIC). */
	ret = sprd_codec_ana_probe(pdev);
	if (ret < 0) {
		pr_err("%s analog probe failed\n", __func__);
		goto clk26m_sinout;
	}

	/* Probe for digital part(in AP). */
	dig_np = of_parse_phandle(np, "digital-codec", 0);
	if (!dig_np) {
		pr_err("%s Parse 'digital-codec' failed\n", __func__);
		ret = -EINVAL;
		goto clk26m_sinout;
	}
	pmu_apb_gpr =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb_gpr)) {
		pr_err("%s Get the pmu apb syscon failed\n", __func__);
		pmu_apb_gpr = NULL;
		ret = -EPROBE_DEFER;
		goto clk26m_sinout;
	}

	mutex_init(&sprd_codec->dig_access_mutex);

	mutex_init(&sprd_codec->digital_enable_mutex);
	ret = of_property_read_u32(np, "set-offset", &set_offset);
	if (ret) {
		sp_asoc_pr_info("%s No set-offset attribute\n", __func__);
		set_offset = 0x100;
	}
	ret = of_property_read_u32(np, "clr-offset", &clr_offset);
	if (ret) {
		sp_asoc_pr_info("%s No clr-offset attribute\n", __func__);
		clr_offset = 0x200;
	}
	sp_asoc_pr_dbg("%s set_offset 0x%x, clr_offset 0x%x\n", __func__,
		       set_offset, clr_offset);
	set_agcp_ahb_offset(set_offset, clr_offset);

	ret = of_property_read_u32(np, "lrdat-sel", &lrdat_sel);
	if (ret) {
		sp_asoc_pr_info("%s Don't need lrdat-sel\n", __func__);
		lrdat_sel = 0x0;
	}
	sprd_codec->lrdat_sel = lrdat_sel;

	arch_audio_set_pmu_apb_gpr(pmu_apb_gpr);
	of_node_put(np);
	dig_pdev = of_find_device_by_node(dig_np);
	if (unlikely(!dig_pdev)) {
		pr_err("%s this node has no pdev?\n", __func__);
		ret = -EPROBE_DEFER;
		goto clk26m_sinout;
	}
	platform_set_drvdata(dig_pdev, sprd_codec);
	ret = sprd_codec_dig_probe(dig_pdev);
	if (ret < 0) {
		pr_err("%s digital probe failed\n", __func__);
		goto clk26m_sinout;
	}

	sprd_codec->ana_chip_id  = sci_get_ana_chip_id();
	sprd_codec->ana_chip_ver_id = sci_get_ana_chip_ver();
	pr_info("ana_chip_id 0x%x, ver_id %x\n", sprd_codec->ana_chip_id,
		sprd_codec->ana_chip_ver_id);
	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_sprd_codec,
				     sprd_codec_dai,
				     ARRAY_SIZE(sprd_codec_dai));
	if (ret != 0) {
		pr_err("Failed to register CODEC %d\n", ret);
		goto clk26m_sinout;
	}
	sprd_codec_inter_pa_init(sprd_codec);
	sprd_codec_power_regulator_init(sprd_codec, &pdev->dev);

	cp_short_check(sprd_codec);
	sprd_codec_init_fgu(sprd_codec);

	ret = sprd_codec_read_efuse(pdev, "neg_cp_efuse",
				    &sprd_codec->neg_cp_efuse);
	if (ret)
		sprd_codec->neg_cp_efuse = 0xe1;
	ret = sprd_codec_read_efuse(pdev, "fgu_4p2_efuse",
				    &sprd_codec->fgu_4p2_efuse);
	if (ret)
		sprd_codec->fgu_4p2_efuse = 0;
	sp_asoc_pr_dbg("%s neg_cp_efuse 0x%x, fgu_4p2_efuse 0x%x\n", __func__,
		       sprd_codec->neg_cp_efuse, sprd_codec->fgu_4p2_efuse);

	ret = sprd_codec_clk26m_sinout(sprd_codec, false);
	if (ret)
		pr_err("%s clean clk26m_sinout_pmic fail\n");

	return 0;

clk26m_sinout:
	/* disable clk26m_sinout if enabled when exit */
	if (sprd_codec_clk26m_sinout(sprd_codec, false))
		pr_err("%s clean clk26m_sinout_pmic fail\n");
error:
	pr_err("%s fail %d\n", __func__, ret);
	return ret;
}

static int sprd_codec_remove(struct platform_device *pdev)
{
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);

	sprd_codec_power_regulator_exit(sprd_codec);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id codec_of_match[] = {
	{.compatible = "sprd,ump9620-audio-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, codec_of_match);
#endif

static struct platform_driver sprd_codec_codec_driver = {
	.driver = {
		.name = "sprd-codec-ump9620",
		.owner = THIS_MODULE,
		.of_match_table = codec_of_match,
	},
	.probe = sprd_codec_probe,
	.remove = sprd_codec_remove,
};

static int __init sprd_codec_driver_init(void)
{
	return platform_driver_register(&sprd_codec_codec_driver);
}

late_initcall(sprd_codec_driver_init);

MODULE_DESCRIPTION("SPRD-CODEC ALSA SoC codec driver");
MODULE_AUTHOR("SPRD");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:sprd-codec");
