/*
 * sound/soc/sprd/codec/sprd/v1/sprd-codec.c
 *
 * SPRD-CODEC -- SpreadTrum Tiger intergrated codec.
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("CDCV1") fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/power_supply.h>
#include <linux/io.h>
#include <linux/of.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/sprd-audio-hook.h>

#include <mach/sprd-audio.h>
#include <mach/globalregs.h>
#include "sprd-codec.h"
#include "sprd-asoc-common.h"

#define SOC_REG(r) ((unsigned short)(r))
#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define ID_FUN(id, lr) ((unsigned short)(((id) << 1) | (lr)))

#define SPRD_CODEC_AP_BASE_HI (SPRD_CODEC_AP_BASE & 0xFFFF0000)
#define SPRD_CODEC_DP_BASE_HI (SPRD_CODEC_DP_BASE & 0xFFFF0000)

enum {
	SPRD_CODEC_PLAYBACK,
	SPRD_CODEC_CAPTRUE,
	SPRD_CODEC_CHAN_MAX,
};

static const char *sprd_codec_chan_name[SPRD_CODEC_CHAN_MAX] = {
	"DAC",
	"ADC",
};

static inline const char *sprd_codec_chan_get_name(int chan_id)
{
	return sprd_codec_chan_name[chan_id];
}

enum {
	SPRD_CODEC_PGA_SPKL = 0,
	SPRD_CODEC_PGA_SPKR,
	SPRD_CODEC_PGA_HPL,
	SPRD_CODEC_PGA_HPR,
	SPRD_CODEC_PGA_EAR,
	SPRD_CODEC_PGA_ADCL,
	SPRD_CODEC_PGA_ADCR,

	SPRD_CODEC_PGA_MAX
};

static const char *sprd_codec_pga_debug_str[SPRD_CODEC_PGA_MAX] = {
	"SPKL",
	"SPKR",
	"HPL",
	"HPR",
	"EAR",
	"ADCL",
	"ADCR",
};

typedef int (*sprd_codec_pga_set) (struct snd_soc_codec * codec, int pgaval);

struct sprd_codec_pga {
	sprd_codec_pga_set set;
	int min;
};

struct sprd_codec_pga_op {
	int pgaval;
	sprd_codec_pga_set set;
};

enum {
	SPRD_CODEC_LEFT = 0,
	SPRD_CODEC_RIGHT = 1,
};

enum {
	SPRD_CODEC_MIXER_START = 0,
	SPRD_CODEC_AIL = SPRD_CODEC_MIXER_START,
	SPRD_CODEC_AIR,
	SPRD_CODEC_MAIN_MIC,
	SPRD_CODEC_AUX_MIC,
	SPRD_CODEC_HP_MIC,
	SPRD_CODEC_ADC_MIXER_MAX,
	SPRD_CODEC_HP_DACL = SPRD_CODEC_ADC_MIXER_MAX,
	SPRD_CODEC_HP_DACR,
	SPRD_CODEC_HP_DAC_MIXER_MAX,
	SPRD_CODEC_HP_ADCL = SPRD_CODEC_HP_DAC_MIXER_MAX,
	SPRD_CODEC_HP_ADCR,
	SPRD_CODEC_HP_MIXER_MAX,
	SPRD_CODEC_SPK_DACL = SPRD_CODEC_HP_MIXER_MAX,
	SPRD_CODEC_SPK_DACR,
	SPRD_CODEC_SPK_DAC_MIXER_MAX,
	SPRD_CODEC_SPK_ADCL = SPRD_CODEC_SPK_DAC_MIXER_MAX,
	SPRD_CODEC_SPK_ADCR,
	SPRD_CODEC_SPK_MIXER_MAX,
	SPRD_CODEC_EAR_DACL = SPRD_CODEC_SPK_MIXER_MAX,
	SPRD_CODEC_EAR_MIXER_MAX,

	SPRD_CODEC_MIXER_MAX = SPRD_CODEC_EAR_MIXER_MAX << SPRD_CODEC_RIGHT
};

static const char *sprd_codec_mixer_debug_str[SPRD_CODEC_MIXER_MAX] = {
	"AIL->ADCL",
	"AIL->ADCR",
	"AIR->ADCL",
	"AIR->ADCR",
	"MAIN MIC->ADCL",
	"MAIN MIC->ADCR",
	"AUX MIC->ADCL",
	"AUX MIC->ADCR",
	"HP MIC->ADCL",
	"HP MIC->ADCR",
	"DACL->HPL",
	"DACL->HPR",
	"DACR->HPL",
	"DACR->HPR",
	"ADCL->HPL",
	"ADCL->HPR",
	"ADCR->HPL",
	"ADCR->HPR",
	"DACL->SPKL",
	"DACL->SPKR",
	"DACR->SPKL",
	"DACR->SPKR",
	"ADCL->SPKL",
	"ADCL->SPKR",
	"ADCR->SPKL",
	"ADCR->SPKR",
	"DACL->EAR",
	"DACR->EAR(bug)"
};

#define IS_SPRD_CODEC_MIXER_RANG(reg) (((reg) >= SPRD_CODEC_MIXER_START) && ((reg) <= SPRD_CODEC_MIXER_MAX))

typedef int (*sprd_codec_mixer_set) (struct snd_soc_codec * codec, int on);
struct sprd_codec_mixer {
	int on;
	sprd_codec_mixer_set set;
};

uint32_t sprd_get_vbat_voltage(void)
    __attribute__ ((weak, alias("__sprd_get_vbat_voltage")));
static uint32_t __sprd_get_vbat_voltage(void)
{
	pr_err("ERR: Can't get vbat!\n");
	return 3800;
}

struct sprd_codec_ldo_v_map {
	int ldo_v_level;
	int volt;
};

const static struct sprd_codec_ldo_v_map ldo_v_map[] = {
	/*{      , 3400}, */
	{LDO_V_29, 3600},
	{LDO_V_31, 3700},
	{LDO_V_32, 3800},
	{LDO_V_33, 3900},
	{LDO_V_34, 4000},
	{LDO_V_35, 4100},
	{LDO_V_36, 4200},
	{LDO_V_38, 4300},
};

const static struct sprd_codec_ldo_v_map ldo_vcom_v_map[] = {
	/*{      , 3400}, */
	{LDO_V_29, 3800},
	{LDO_V_31, 3900},
	{LDO_V_32, 4000},
	{LDO_V_33, 4100},
	{LDO_V_34, 4200},
	{LDO_V_35, 4300},
	{LDO_V_36, 4400},
	{LDO_V_38, 4500},
};

struct sprd_codec_ldo_cfg {
	const struct sprd_codec_ldo_v_map *v_map;
	int v_map_size;
};

struct sprd_codec_inter_pa {
	/* FIXME little endian */
	int LDO_V_sel:4;
	int DTRI_F_sel:4;
	int is_DEMI_mode:1;
	int is_classD_mode:1;
	int is_LDO_mode:1;
	int is_auto_LDO_mode:1;
	int RESV:20;
};

struct sprd_codec_pa_setting {
	union {
		struct sprd_codec_inter_pa setting;
		u32 value;
	};
	int set;
};

enum {
	SPRD_CODEC_MIC_BIAS,
	SPRD_CODEC_AUXMIC_BIAS,
	SPRD_CODEC_MIC_BIAS_MAX
};

static const char *mic_bias_name[SPRD_CODEC_MIC_BIAS_MAX] = {
	"Mic Bias",
	"AuxMic Bias",
};

/* codec private data */
struct sprd_codec_priv {
	struct snd_soc_codec *codec;
	atomic_t adie_dac_refcount;
	atomic_t adie_adc_refcount;
	int da_sample_val;
	int ad_sample_val;
	struct sprd_codec_mixer mixer[SPRD_CODEC_MIXER_MAX];
	struct sprd_codec_pga_op pga[SPRD_CODEC_PGA_MAX];
	int mic_bias[SPRD_CODEC_MIC_BIAS_MAX];

	int ap_irq;
	struct completion completion_hp_pop;

	int dp_irq;
	struct completion completion_dac_mute;
	struct power_supply audio_ldo;

	struct sprd_codec_pa_setting inter_pa;
	struct mutex inter_pa_mutex;

	struct regulator *main_mic;
	struct regulator *aux_mic;
	atomic_t ldo_refcount;
	struct delayed_work ovp_delayed_work;

	struct sprd_codec_ldo_cfg s_vcom;
	struct sprd_codec_ldo_cfg s_vpa;

#define SPRD_CODEC_PA_SW_AOL (BIT(0))
#define SPRD_CODEC_PA_SW_EAR (BIT(1))
#define SPRD_CODEC_PA_SW_FUN (SPRD_CODEC_PA_SW_AOL | SPRD_CODEC_PA_SW_EAR)
	int sprd_codec_fun;
	spinlock_t sprd_codec_fun_lock;
	spinlock_t sprd_codec_pa_sw_lock;
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
			return PTR_ERR(*regu);
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

static int sprd_codec_mic_bias_set(struct regulator **regu, int on)
{
	int ret = 0;
	if (*regu) {
		if (on) {
			ret = regulator_enable(*regu);
			if (ret) {
				sprd_codec_power_put(regu);
				return ret;
			}
		} else {
			ret = regulator_disable(*regu);
		}
	}
	return ret;
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

static void sprd_codec_set_fun(struct sprd_codec_priv *sprd_codec, int fun)
{
	spin_lock(&sprd_codec->sprd_codec_fun_lock);
	sprd_codec->sprd_codec_fun |= fun;
	spin_unlock(&sprd_codec->sprd_codec_fun_lock);
}

static void sprd_codec_clr_fun(struct sprd_codec_priv *sprd_codec, int fun)
{
	spin_lock(&sprd_codec->sprd_codec_fun_lock);
	sprd_codec->sprd_codec_fun &= ~fun;
	spin_unlock(&sprd_codec->sprd_codec_fun_lock);
}

static int sprd_codec_test_fun(struct sprd_codec_priv *sprd_codec, int fun)
{
	int ret;
	spin_lock(&sprd_codec->sprd_codec_fun_lock);
	ret = sprd_codec->sprd_codec_fun & fun;
	spin_unlock(&sprd_codec->sprd_codec_fun_lock);
	return ret;
}

static void sprd_codec_wait(u32 wait_time)
{
	if (wait_time)
		schedule_timeout_uninterruptible(msecs_to_jiffies(wait_time));
}

#if 0				/* sometime we will use this print function */
static unsigned int sprd_codec_read(struct snd_soc_codec *codec,
				    unsigned int reg);
static void sprd_codec_print_regs(struct snd_soc_codec *codec)
{
	int reg;
	pr_warn("sprd_codec register digital part\n");
	for (reg = SPRD_CODEC_DP_BASE; reg < SPRD_CODEC_DP_END; reg += 0x10) {
		pr_warn("0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(reg - SPRD_CODEC_DP_BASE)
			, sprd_codec_read(codec, reg + 0x00)
			, sprd_codec_read(codec, reg + 0x04)
			, sprd_codec_read(codec, reg + 0x08)
			, sprd_codec_read(codec, reg + 0x0C)
		    );
	}
	pr_warn("sprd_codec register analog part\n");
	for (reg = SPRD_CODEC_AP_BASE; reg < SPRD_CODEC_AP_END; reg += 0x10) {
		pr_warn("0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(reg - SPRD_CODEC_AP_BASE)
			, sprd_codec_read(codec, reg + 0x00)
			, sprd_codec_read(codec, reg + 0x04)
			, sprd_codec_read(codec, reg + 0x08)
			, sprd_codec_read(codec, reg + 0x0C)
		    );
	}
}
#endif

static inline int _sprd_codec_ldo_cfg_set(struct sprd_codec_ldo_cfg *p_v_cfg, const struct sprd_codec_ldo_v_map
					  *v_map, const int v_map_size)
{
	p_v_cfg->v_map = v_map;
	p_v_cfg->v_map_size = v_map_size;
	return 0;
}

static inline int _sprd_codec_hold(int index, int volt,
				   const struct sprd_codec_ldo_v_map *v_map)
{
	int scope = 40;		/* unit: mv */
	int ret = ((v_map[index].volt - volt) <= scope);
	if (index >= 1) {
		return (ret || ((volt - v_map[index - 1].volt) <= scope));
	}
	return ret;
}

static int sprd_codec_auto_ldo_volt(struct snd_soc_codec *codec,
				    void (*set_level) (struct snd_soc_codec *,
						       int),
				    const struct sprd_codec_ldo_v_map *v_map,
				    const int v_map_size, int init)
{
	int i;
	int volt = sprd_get_vbat_voltage();
	sp_asoc_pr_dbg("Get vbat voltage %d\n", volt);
	for (i = 0; i < v_map_size; i++) {
		if (volt <= ldo_v_map[i].volt) {
			sp_asoc_pr_dbg("Hold %d\n",
				       _sprd_codec_hold(i, volt, v_map));
			if (init || !_sprd_codec_hold(i, volt, v_map)) {
				set_level(codec, v_map[i].ldo_v_level);
			}
			return 0;
		}
	}
	return -EFAULT;
}

static inline void sprd_codec_vcm_v_sel(struct snd_soc_codec *codec, int v_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("VCM Set %d\n", v_sel);
	mask = VCM_V_MASK << VCM_V;
	val = (v_sel << VCM_V) & mask;
	snd_soc_update_bits(codec, SOC_REG(PMUR3), mask, val);
}

static int sprd_codec_pga_spk_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR3;
	val = (pgaval & 0xF) << 4;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_spkr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR3;
	val = pgaval & 0xF;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x0F, val);
}

static int sprd_codec_pga_hpl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR1;
	val = (pgaval & 0xF) << 4;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_hpr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR1;
	val = pgaval & 0xF;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x0F, val);
}

static int sprd_codec_pga_ear_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR2;
	val = ((pgaval & 0xF) << 4);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_adcl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = ACGR;
	val = (pgaval & 0xF) << 4;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_adcr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = ACGR;
	val = pgaval & 0xF;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x0F, val);
}

static struct sprd_codec_pga sprd_codec_pga_cfg[SPRD_CODEC_PGA_MAX] = {
	{sprd_codec_pga_spk_set, 0},
	{sprd_codec_pga_spkr_set, 0},
	{sprd_codec_pga_hpl_set, 0},
	{sprd_codec_pga_hpr_set, 0},
	{sprd_codec_pga_ear_set, 0},

	{sprd_codec_pga_adcl_set, 0},
	{sprd_codec_pga_adcr_set, 0},
};

/* adc mixer */

static int vcmadcl_set(struct snd_soc_codec *codec)
{
	int need_on;
	need_on = snd_soc_read(codec, AAICR3) & (BIT(AIL_ADCL) | BIT(AIR_ADCL));
	return snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(VCM_ADCL),
				   (! !need_on) << VCM_ADCL);
}

static int vcmadcr_set(struct snd_soc_codec *codec)
{
	int need_on;
	need_on = snd_soc_read(codec, AAICR3) & (BIT(AIL_ADCR) | BIT(AIR_ADCR));
	return snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(VCM_ADCR),
				   (! !need_on) << VCM_ADCR);
}

static int ailadcl_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIL_ADCL),
				  on << AIL_ADCL);
	if (ret < 0)
		return ret;
	return vcmadcl_set(codec);
}

static int ailadcr_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIL_ADCR),
				  on << AIL_ADCR);
	if (ret < 0)
		return ret;
	return vcmadcr_set(codec);
}

static int airadcl_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIR_ADCL),
				  on << AIR_ADCL);
	if (ret < 0)
		return ret;
	return vcmadcl_set(codec);
}

static int airadcr_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIR_ADCR),
				  on << AIR_ADCR);
	if (ret < 0)
		return ret;
	return vcmadcr_set(codec);
}

static int mainmicadcl_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	mask = BIT(MICP_ADCL) | BIT(MICN_ADCL);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

static int mainmicadcr_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	mask = BIT(MICP_ADCR) | BIT(MICN_ADCR);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

static int auxmicadcl_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	mask = BIT(AUXMICP_ADCL) | BIT(AUXMICN_ADCL);
	return snd_soc_update_bits(codec, SOC_REG(AAICR2), mask, on ? mask : 0);
}

static int auxmicadcr_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	mask = BIT(AUXMICP_ADCR) | BIT(AUXMICN_ADCR);
	return snd_soc_update_bits(codec, SOC_REG(AAICR2), mask, on ? mask : 0);
}

static int hpmicadcl_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	mask = BIT(HPMICP_ADCL) | BIT(HPMICN_ADCL);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

static int hpmicadcr_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	mask = BIT(HPMICP_ADCR) | BIT(HPMICN_ADCR);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

/* adcpga*_en setting */
static int adcpgax_auto_setting(struct snd_soc_codec *codec)
{
	unsigned int reg1, reg2, reg3;
	int val;
	int mask;
	reg1 = snd_soc_read(codec, DAOCR1);
	reg2 = snd_soc_read(codec, DAOCR2);
	reg3 = snd_soc_read(codec, DAOCR3);
	val = 0;
	if ((reg1 & BIT(ADCL_P_HPL)) || (reg2 & BIT(ADCL_P_AOLP))
	    || (reg3 & BIT(ADCL_P_AORP))) {
		val |= BIT(ADCPGAL_P_EN);
	}
	if ((reg1 & BIT(ADCL_N_HPR)) || (reg2 & BIT(ADCL_N_AOLN))
	    || (reg3 & BIT(ADCL_N_AORN))) {
		val |= BIT(ADCPGAL_N_EN);
	}
	if ((reg1 & (BIT(ADCR_P_HPL) | BIT(ADCR_P_HPR)))
	    || (reg2 & BIT(ADCR_P_AOLP)) || (reg3 & BIT(ADCR_P_AORP))) {
		val |= BIT(ADCPGAR_P_EN);
	}
	if ((reg2 & BIT(ADCR_N_AOLN)) || (reg3 & BIT(ADCR_N_AORN))) {
		val |= BIT(ADCPGAR_N_EN);
	}
	mask =
	    BIT(ADCPGAL_P_EN) | BIT(ADCPGAL_N_EN) | BIT(ADCPGAR_P_EN) |
	    BIT(ADCPGAR_N_EN);
	return snd_soc_update_bits(codec, SOC_REG(AACR2), mask, val);
}

/* hp mixer */

static int daclhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACL_P_HPL),
				   on << DACL_P_HPL);
}

static int daclhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACL_N_HPR),
				   on << DACL_N_HPR);
}

static int dacrhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACR_P_HPL),
				   on << DACR_P_HPL);
}

static int dacrhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACR_P_HPR),
				   on << DACR_P_HPR);
}

static int adclhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCL_P_HPL),
			    on << ADCL_P_HPL);
	return adcpgax_auto_setting(codec);
}

static int adclhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCL_N_HPR),
			    on << ADCL_N_HPR);
	return adcpgax_auto_setting(codec);
}

static int adcrhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCR_P_HPL),
			    on << ADCR_P_HPL);
	return adcpgax_auto_setting(codec);
}

static int adcrhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCR_P_HPR),
			    on << ADCR_P_HPR);
	return adcpgax_auto_setting(codec);
}

/* spkl mixer */

static int daclspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACL_P_AOLP) | BIT(DACL_N_AOLN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
}

static int dacrspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACR_P_AOLP) | BIT(DACR_N_AOLN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
}

static int adclspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCL_P_AOLP) | BIT(ADCL_N_AOLN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

static int adcrspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCR_P_AOLP) | BIT(ADCR_N_AOLN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

/* spkr mixer */

static int daclspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACL_P_AORP) | BIT(DACL_N_AORN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
}

static int dacrspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACR_P_AORP) | BIT(DACR_N_AORN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
}

static int adclspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCL_P_AORP) | BIT(ADCL_N_AORN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

static int adcrspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCR_P_AORP) | BIT(ADCR_N_AORN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

/* ear mixer */

static int daclear_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACL_P_EARP) | BIT(DACL_N_EARN);
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR4), mask, on ? mask : 0);
}

static sprd_codec_mixer_set mixer_setting[SPRD_CODEC_MIXER_MAX] = {
	/* adc mixer */
	ailadcl_set, ailadcr_set,
	airadcl_set, airadcr_set,
	mainmicadcl_set, mainmicadcr_set,
	auxmicadcl_set, auxmicadcr_set,
	hpmicadcl_set, hpmicadcr_set,
	/* hp mixer */
	daclhpl_set, daclhpr_set,
	dacrhpl_set, dacrhpr_set,
	adclhpl_set, adclhpr_set,
	adcrhpl_set, adcrhpr_set,
	/* spk mixer */
	daclspkl_set, daclspkr_set,
	dacrspkl_set, dacrspkr_set,
	adclspkl_set, adclspkr_set,
	adcrspkl_set, adcrspkr_set,
	/* ear mixer */
	daclear_set, 0,
};

/* DO NOT USE THIS FUNCTION */
static inline void __sprd_codec_pa_sw_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	mask = BIT(PA_SW_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(PMUR2), mask, val);
}

static inline void sprd_codec_pa_sw_set(struct snd_soc_codec *codec, int fun)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	sp_asoc_pr_dbg("%s fun 0x%08x\n", __func__, fun);
	spin_lock(&sprd_codec->sprd_codec_pa_sw_lock);
	sprd_codec_set_fun(sprd_codec, fun);
	__sprd_codec_pa_sw_en(codec, 1);
	spin_unlock(&sprd_codec->sprd_codec_pa_sw_lock);
}

static inline void sprd_codec_pa_sw_clr(struct snd_soc_codec *codec, int fun)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	sp_asoc_pr_dbg("%s fun 0x%08x\n", __func__, fun);
	spin_lock(&sprd_codec->sprd_codec_pa_sw_lock);
	sprd_codec_clr_fun(sprd_codec, fun);
	if (!sprd_codec_test_fun(sprd_codec, SPRD_CODEC_PA_SW_FUN))
		__sprd_codec_pa_sw_en(codec, 0);
	spin_unlock(&sprd_codec->sprd_codec_pa_sw_lock);
}

/* inter PA */

static inline void sprd_codec_pa_d_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	mask = BIT(PA_D_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(DCR2), mask, val);
}

static inline void sprd_codec_pa_demi_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	mask = BIT(PA_DEMI_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(DCR2), mask, val);

	mask = BIT(DRV_OCP_AOL_PD) | BIT(DRV_OCP_AOR_PD);
	val = mask;
	snd_soc_update_bits(codec, SOC_REG(DCR3), mask, val);
}

static inline void sprd_codec_pa_ldo_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	mask = BIT(PA_LDO_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(PMUR2), mask, val);
	if (on) {
		sprd_codec_pa_sw_clr(codec, SPRD_CODEC_PA_SW_AOL);
	}
}

static inline void sprd_codec_pa_ldo_v_sel(struct snd_soc_codec *codec,
					   int v_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, v_sel);
	mask = PA_LDO_V_MASK << PA_LDO_V;
	val = (v_sel << PA_LDO_V) & mask;
	snd_soc_update_bits(codec, SOC_REG(PMUR4), mask, val);
}

static inline void sprd_codec_pa_ovp_v_sel(struct snd_soc_codec *codec,
					   int v_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, v_sel);
	mask = PA_OVP_V_MASK << PA_OVP_V;
	val = (v_sel << PA_OVP_V) & mask;
	snd_soc_update_bits(codec, SOC_REG(PMUR6), mask, val);
	snd_soc_update_bits(codec, SOC_REG(AUDIF_OVPTMR_CTL), 0x1A, 0x3F);
}

static inline void sprd_codec_pa_dtri_f_sel(struct snd_soc_codec *codec,
					    int f_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, f_sel);
	mask = PA_DTRI_F_MASK << PA_DTRI_F;
	val = (f_sel << PA_DTRI_F) & mask;
	snd_soc_update_bits(codec, SOC_REG(DCR2), mask, val);
}

static inline void sprd_codec_pa_en(struct snd_soc_codec *codec, int on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	spin_lock(&sprd_codec->sprd_codec_pa_sw_lock);
	if (on) {
		mask = BIT(PA_EN);
		val = mask;
	} else {
		if (!sprd_codec_test_fun(sprd_codec, SPRD_CODEC_PA_SW_FUN))
			mask = BIT(PA_EN) | BIT(PA_SW_EN) | BIT(PA_LDO_EN);
		else
			mask = BIT(PA_EN) | BIT(PA_LDO_EN);
		val = 0;
	}
	snd_soc_update_bits(codec, SOC_REG(PMUR2), mask, val);
	spin_unlock(&sprd_codec->sprd_codec_pa_sw_lock);
}

static inline void sprd_codec_inter_pa_init(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec->inter_pa.setting.LDO_V_sel = 0x03;
	sprd_codec->inter_pa.setting.DTRI_F_sel = 0x01;
}

static void sprd_codec_ovp_irq_enable(struct snd_soc_codec *codec)
{
	int mask = BIT(OVP_IRQ);
	snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_CLR), mask, mask);
	snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_EN), mask, mask);
}

static void sprd_codec_ovp_start(struct sprd_codec_priv *sprd_codec, int ms)
{
	schedule_delayed_work(&sprd_codec->ovp_delayed_work,
			      msecs_to_jiffies(ms));
}

static int sprd_ovp_irq(struct snd_soc_codec *codec, int is_ovp)
{
	int volt = sprd_get_vbat_voltage();
	sp_asoc_pr_dbg("OVP %d curr_volt %d\n", is_ovp, volt);
	if (is_ovp) {
		sprd_codec_pa_en(codec, 0);
		snd_soc_update_bits(codec, SOC_REG(DCR1), BIT(AOL_EN), 0);
	} else {
		snd_soc_update_bits(codec, SOC_REG(DCR1), BIT(AOL_EN),
				    BIT(AOL_EN));
		sprd_codec_pa_en(codec, 1);
	}
	return 0;
}

#define to_sprd_codec(x) container_of((x), struct sprd_codec_priv, ovp_delayed_work)
#define to_delay_worker(x) container_of((x), struct delayed_work, work)
static void sprd_codec_ovp_delay_worker(struct work_struct *work)
{
	struct sprd_codec_priv *sprd_codec =
	    to_sprd_codec(to_delay_worker(work));
	struct snd_soc_codec *codec = sprd_codec->codec;

	if (!(snd_soc_read(codec, IFR1) & BIT(OVP_FLAG))) {
		sprd_ovp_irq(codec, 0);
	} else {
		sprd_ovp_irq(codec, 1);
		sprd_codec_ovp_start(sprd_codec, 500);
	}
}

static inline int sprd_codec_pa_ldo_auto(struct snd_soc_codec *codec, int init)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	return sprd_codec_auto_ldo_volt
	    (codec, sprd_codec_pa_ldo_v_sel, sprd_codec->s_vpa.v_map,
	     sprd_codec->s_vpa.v_map_size, init);
}

static inline int sprd_codec_pa_ldo_cfg(struct sprd_codec_priv *sprd_codec, const struct sprd_codec_ldo_v_map
					*v_map, const int v_map_size)
{
	return _sprd_codec_ldo_cfg_set(&sprd_codec->s_vpa, v_map, v_map_size);
}

static int sprd_inter_speaker_pa(struct snd_soc_codec *codec, int on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct sprd_codec_inter_pa *p_setting = &sprd_codec->inter_pa.setting;
	sp_asoc_pr_info("inter PA Switch %s\n", STR_ON_OFF(on));
	mutex_lock(&sprd_codec->inter_pa_mutex);
	if (on) {
		sprd_codec_pa_ovp_v_sel(codec, PA_OVP_465);
		sprd_codec_ovp_irq_enable(codec);
		sprd_codec_pa_d_en(codec, p_setting->is_classD_mode);
		sprd_codec_pa_demi_en(codec, p_setting->is_DEMI_mode);
		sprd_codec_pa_ldo_en(codec, p_setting->is_LDO_mode);
		if (p_setting->is_LDO_mode) {
			if (p_setting->is_auto_LDO_mode) {
				sprd_codec_pa_ldo_auto(codec, 1);
			} else {
				sprd_codec_pa_ldo_v_sel(codec,
							p_setting->LDO_V_sel);
			}
		} else {
			sprd_codec_pa_sw_set(codec, SPRD_CODEC_PA_SW_AOL);
		}
		sprd_codec_pa_dtri_f_sel(codec, p_setting->DTRI_F_sel);
		sprd_codec_pa_en(codec, 1);
		sprd_codec->inter_pa.set = 1;
	} else {
		sprd_codec->inter_pa.set = 0;
		sprd_codec_pa_en(codec, 0);
		sprd_codec_wait(2);
		sprd_codec_pa_sw_clr(codec, SPRD_CODEC_PA_SW_AOL);
		sprd_codec_pa_ldo_en(codec, 0);
	}
	mutex_unlock(&sprd_codec->inter_pa_mutex);
	return 0;
}

static int spk_pa_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	return sprd_inter_speaker_pa(w->codec, on);
}

static int sprd_codec_set_sample_rate(struct snd_soc_codec *codec, int rate,
				      int mask, int shift)
{
	switch (rate) {
	case 8000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_8000 << shift);
		break;
	case 11025:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_11025 << shift);
		break;
	case 16000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_16000 << shift);
		break;
	case 22050:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_22050 << shift);
		break;
	case 32000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_32000 << shift);
		break;
	case 44100:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_44100 << shift);
		break;
	case 48000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_48000 << shift);
		break;
	case 96000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_96000 << shift);
		break;
	default:
		pr_err("ERR:SPRD-CODEC not support this rate %d\n", rate);
		break;
	}
	sp_asoc_pr_dbg("Set Playback rate 0x%x\n",
		       snd_soc_read(codec, AUD_DAC_CTL));
	return 0;
}

static int sprd_codec_set_ad_sample_rate(struct snd_soc_codec *codec, int rate,
					 int mask, int shift)
{
	int set;
	set = rate / 4000;
	if (set > 13) {
		pr_err("ERR:SPRD-CODEC not support this ad rate %d\n", rate);
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC_CTL), mask, set << shift);
	return 0;
}

static int sprd_codec_sample_rate_setting(struct sprd_codec_priv *sprd_codec)
{
	sp_asoc_pr_dbg("%s AD %d DA %d \n", __func__,
		       sprd_codec->ad_sample_val, sprd_codec->da_sample_val);
	if (sprd_codec->ad_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
					      sprd_codec->ad_sample_val, 0x0F,
					      0);
	}
	if (sprd_codec->da_sample_val) {
		sprd_codec_set_sample_rate(sprd_codec->codec,
					   sprd_codec->da_sample_val, 0x0F, 0);
	}
	return 0;
}

static inline int sprd_codec_vcom_ldo_auto(struct snd_soc_codec *codec,
					   int init)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	return sprd_codec_auto_ldo_volt(codec, sprd_codec_vcm_v_sel,
					sprd_codec->s_vcom.v_map,
					sprd_codec->s_vcom.v_map_size, init);
}

static inline int sprd_codec_vcom_ldo_cfg(struct sprd_codec_priv *sprd_codec, const struct sprd_codec_ldo_v_map
					  *v_map, const int v_map_size)
{
	return _sprd_codec_ldo_cfg_set(&sprd_codec->s_vcom, v_map, v_map_size);
}

static DEFINE_MUTEX(ldo_mutex);
static int sprd_codec_ldo_on(struct sprd_codec_priv *sprd_codec)
{
	sp_asoc_pr_dbg("%s\n", __func__);

	mutex_lock(&ldo_mutex);
	atomic_inc(&sprd_codec->ldo_refcount);
	if (atomic_read(&sprd_codec->ldo_refcount) == 1) {
		sp_asoc_pr_dbg("LDO ON!\n");
		arch_audio_codec_analog_enable();
		sprd_codec_vcom_ldo_auto(sprd_codec->codec, 1);
	}
	mutex_unlock(&ldo_mutex);

	return 0;
}

static int sprd_codec_ldo_off(struct sprd_codec_priv *sprd_codec)
{
	sp_asoc_pr_dbg("%s\n", __func__);

	mutex_lock(&ldo_mutex);
	if (atomic_dec_and_test(&sprd_codec->ldo_refcount)) {
		arch_audio_codec_analog_disable();
		sp_asoc_pr_dbg("LDO OFF!\n");
	}
	mutex_unlock(&ldo_mutex);

	return 0;
}

static int sprd_codec_analog_open(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s\n", __func__);
	sprd_codec_sample_rate_setting(sprd_codec);

	/* SC8825 ask from ASIC to set initial value */
	snd_soc_write(codec, AUD_SDM_CTL0, 0x400);
	snd_soc_write(codec, AUD_SDM_CTL1, 0);

	return ret;
}

static int sprd_codec_digital_open(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_dbg("%s\n", __func__);

	/* FIXME : Some Clock SYNC bug will cause MUTE */
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), BIT(DAC_MUTE_EN), 0);

	sprd_codec_sample_rate_setting(sprd_codec);

	return ret;
}

static void sprd_codec_power_enable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret;

	sp_asoc_pr_dbg("%s\n", __func__);

	ret = sprd_codec_ldo_on(sprd_codec);
	if (ret != 0)
		pr_err("ERR:SPRD-CODEC open ldo error %d\n", ret);

	sprd_codec_analog_open(codec);
}

static void sprd_codec_power_disable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s\n", __func__);

	sprd_codec_ldo_off(sprd_codec);
}

static int digital_power_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		arch_audio_codec_digital_reg_enable();
		arch_audio_codec_digital_reset();
		sprd_codec_digital_open(w->codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		arch_audio_codec_digital_reg_disable();
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int analog_power_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_power_enable(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_power_disable(codec);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int chan_event(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol, int event)
{
	int chan_id = FUN_REG(w->reg);
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("%s %s\n", sprd_codec_chan_get_name(chan_id),
			STR_ON_OFF(on));

	return 0;
}

static int adie_dac_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		atomic_inc(&sprd_codec->adie_dac_refcount);
		if (atomic_read(&sprd_codec->adie_dac_refcount) == 1) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_DAC_EN),
					    BIT(AUDIFA_DAC_EN));
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_dec_and_test(&sprd_codec->adie_dac_refcount)) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_DAC_EN), 0);
		}
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int adie_adc_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		atomic_inc(&sprd_codec->adie_adc_refcount);
		if (atomic_read(&sprd_codec->adie_adc_refcount) == 1) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_ADC_EN),
					    BIT(AUDIFA_ADC_EN));
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_dec_and_test(&sprd_codec->adie_adc_refcount)) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_ADC_EN), 0);
		}
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int dfm_out_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("DFM-OUT %s\n", STR_ON_OFF(on));

	return 0;
}
static int _mixer_set_mixer(struct snd_soc_codec *codec, int id, int lr,
			    int try_on, int need_set)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int reg = ID_FUN(id, lr);
	struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[reg]);
	if (try_on) {
		mixer->set = mixer_setting[reg];
		/* NOTES: reduce linein pop noise must open ADCL/R ->HP MIXER
		   AFTER delay 250ms for both ADCL/ADCR switch complete. */
		if (need_set)
			return mixer->set(codec, mixer->on);
	} else {
		mixer_setting[reg] (codec, 0);
		mixer->set = 0;
	}
	return 0;
}

static inline int _mixer_setting(struct snd_soc_codec *codec, int start,
				 int end, int lr, int try_on, int need_set)
{
	int id;
	for (id = start; id < end; id++) {
		_mixer_set_mixer(codec, id, lr, try_on, need_set);
	}
	return 0;
}

static inline int _mixer_setting_one(struct snd_soc_codec *codec, int id,
				     int try_on, int need_set)
{
	int lr = id & 0x1;
	id >>= 1;
	return _mixer_setting(codec, id, id + 1, lr, try_on, need_set);
}

#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
static void sprd_codec_hp_pop_irq_enable(struct snd_soc_codec *codec)
{
	int mask = BIT(AUDIO_POP_IRQ);
	snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_CLR), mask, mask);
	snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_EN), mask, mask);
}
#endif

static irqreturn_t sprd_codec_ap_irq(int irq, void *dev_id)
{
	int mask;
	struct sprd_codec_priv *sprd_codec = dev_id;
	struct snd_soc_codec *codec = sprd_codec->codec;
	mask = snd_soc_read(codec, AUDIF_INT_MASK);
	sp_asoc_pr_dbg("HP POP IRQ Mask = 0x%x\n", mask);
	if (BIT(AUDIO_POP_IRQ) & mask) {
		mask = BIT(AUDIO_POP_IRQ);
		snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_EN), mask, 0);
		complete(&sprd_codec->completion_hp_pop);
	}
	if (BIT(OVP_IRQ) & mask) {
		mask = BIT(OVP_IRQ);
		snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_EN), mask, 0);
		sprd_codec_ovp_start(sprd_codec, 1);
		sprd_codec_ovp_irq_enable(codec);
	}
	return IRQ_HANDLED;
}

#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
static inline int is_hp_pop_compelet(struct snd_soc_codec *codec)
{
	int val;
	val = snd_soc_read(codec, IFR1);
	val = (val >> HP_POP_FLG) & HP_POP_FLG_MASK;
	sp_asoc_pr_dbg("HP POP= 0x%x\n", val);
	return HP_POP_FLG_NEAR_CMP == val;
}

static inline int hp_pop_wait_for_compelet(struct snd_soc_codec *codec)
{
	int i;
	int hp_pop_complete;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	hp_pop_complete = msecs_to_jiffies(SPRD_CODEC_HP_POP_TIMEOUT);
	for (i = 0; i < 2; i++) {
		sp_asoc_pr_dbg("HP POP %d IRQ Enable\n", i);
		sprd_codec_hp_pop_irq_enable(codec);
		init_completion(&sprd_codec->completion_hp_pop);
		hp_pop_complete =
		    wait_for_completion_timeout(&sprd_codec->completion_hp_pop,
						hp_pop_complete);
		sp_asoc_pr_dbg("HP POP %d Completion %d\n", i, hp_pop_complete);
		if (!hp_pop_complete) {
			if (!is_hp_pop_compelet(codec)) {
				pr_err("ERR:HP POP %d timeout not complete\n",
				       i);
			} else {
				pr_err("ERR:HP POP %d timeout but complete\n",
				       i);
			}
		} else {
			/* 01 change to 10 maybe walk to 11 shortly,
			   so, check it double. */
			sprd_codec_wait(2);
			if (is_hp_pop_compelet(codec)) {
				return 0;
			}
		}
	}
	return 0;
}

static int hp_pop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int mask;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mask = HP_POP_STEP_MASK << HP_POP_STEP;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_STEP_2 << HP_POP_STEP);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_UP << HP_POP_CTL);
		sp_asoc_pr_dbg("U PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mask = HP_POP_STEP_MASK << HP_POP_STEP;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_STEP_1 << HP_POP_STEP);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_DOWN << HP_POP_CTL);
		sp_asoc_pr_dbg("D PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	case SND_SOC_DAPM_POST_PMU:
		ret = hp_pop_wait_for_compelet(codec);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_HOLD << HP_POP_CTL);
		sp_asoc_pr_dbg("HOLD PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = hp_pop_wait_for_compelet(codec);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_DIS << HP_POP_CTL);
		sp_asoc_pr_dbg("DIS PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}
#else
#ifndef CONFIG_SND_SOC_SPRD_HP_POP_DELAY_TIME
#define CONFIG_SND_SOC_SPRD_HP_POP_DELAY_TIME (0)
#endif
static int hp_pop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	sp_asoc_pr_dbg("%s Event is %s wait %dms\n", __func__,
		       get_event_name(event),
		       CONFIG_SND_SOC_SPRD_HP_POP_DELAY_TIME);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		sprd_codec_wait(CONFIG_SND_SOC_SPRD_HP_POP_DELAY_TIME);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}
	return ret;
}
#endif

static int hp_switch_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
	int mask;
#endif
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
#if 0				/* do not enable the diff function from weifeng.ni */
		snd_soc_update_bits(codec, SOC_REG(DCR1), BIT(DIFF_EN),
				    BIT(DIFF_EN));
#endif

#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_DIS << HP_POP_CTL);
		sp_asoc_pr_dbg("DIS(en) PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(DCR1), BIT(DIFF_EN), 0);

#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_HOLD << HP_POP_CTL);
		sp_asoc_pr_dbg("HOLD(en) PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
#endif
		goto _pre_pmd;
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* do nothing now */
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	_mixer_setting(codec, SPRD_CODEC_HP_DACL,
		       SPRD_CODEC_HP_DAC_MIXER_MAX, SPRD_CODEC_LEFT,
		       snd_soc_read(codec, DCR1) & BIT(HPL_EN), 1);

	_mixer_setting(codec, SPRD_CODEC_HP_DACL,
		       SPRD_CODEC_HP_DAC_MIXER_MAX, SPRD_CODEC_RIGHT,
		       snd_soc_read(codec, DCR1) & BIT(HPR_EN), 1);

	_mixer_setting(codec, SPRD_CODEC_HP_ADCL,
		       SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_LEFT,
		       snd_soc_read(codec, DCR1) & BIT(HPL_EN), 0);

	_mixer_setting(codec, SPRD_CODEC_HP_ADCL,
		       SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_RIGHT,
		       snd_soc_read(codec, DCR1) & BIT(HPR_EN), 0);

_pre_pmd:

	return ret;
}

static int spk_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (snd_soc_read(codec, DCR1) & BIT(AOL_EN)) {
		switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			sprd_codec_pa_sw_set(codec, SPRD_CODEC_PA_SW_AOL);
			break;
		case SND_SOC_DAPM_PRE_PMD:
			sprd_codec_pa_sw_clr(codec, SPRD_CODEC_PA_SW_AOL);
			return 0;
		default:
			break;
		}
	}

	_mixer_setting(codec, SPRD_CODEC_SPK_DACL,
		       SPRD_CODEC_SPK_DAC_MIXER_MAX, SPRD_CODEC_LEFT,
		       (snd_soc_read(codec, DCR1) & BIT(AOL_EN)), 1);

	_mixer_setting(codec, SPRD_CODEC_SPK_DACL,
		       SPRD_CODEC_SPK_DAC_MIXER_MAX, SPRD_CODEC_RIGHT,
		       (snd_soc_read(codec, DCR1) & BIT(AOR_EN)), 1);

	_mixer_setting(codec, SPRD_CODEC_SPK_ADCL,
		       SPRD_CODEC_SPK_MIXER_MAX, SPRD_CODEC_LEFT,
		       (snd_soc_read(codec, DCR1) & BIT(AOL_EN)), 0);

	_mixer_setting(codec, SPRD_CODEC_SPK_ADCL,
		       SPRD_CODEC_SPK_MIXER_MAX, SPRD_CODEC_RIGHT,
		       (snd_soc_read(codec, DCR1) & BIT(AOR_EN)), 0);

	return 0;
}

#ifdef CONFIG_SND_SOC_SPRD_CODEC_EAR_WITH_IN_SPK
static int ear_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_pa_sw_set(codec, SPRD_CODEC_PA_SW_EAR);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_pa_sw_clr(codec, SPRD_CODEC_PA_SW_EAR);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}
#endif

static int adc_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	_mixer_setting(codec, SPRD_CODEC_AIL, SPRD_CODEC_ADC_MIXER_MAX,
		       SPRD_CODEC_LEFT,
		       (!(snd_soc_read(codec, AACR2) & BIT(ADCPGAL_PD))), 1);

	_mixer_setting(codec, SPRD_CODEC_AIL, SPRD_CODEC_ADC_MIXER_MAX,
		       SPRD_CODEC_RIGHT,
		       (!(snd_soc_read(codec, AACR2) & BIT(ADCPGAR_PD))), 1);

	return 0;
}

static int pga_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(w->reg);
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[id]);
	int ret = 0;
	int min = sprd_codec_pga_cfg[id].min;

	sp_asoc_pr_dbg("%s Set %s(%d) Event is %s\n", __func__,
		       sprd_codec_pga_debug_str[id], pga->pgaval,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pga->set = sprd_codec_pga_cfg[id].set;
		ret = pga->set(codec, pga->pgaval);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pga->set = 0;
		ret = sprd_codec_pga_cfg[id].set(codec, min);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int ana_loop_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(w->reg);
	struct sprd_codec_mixer *mixer;
	int ret = 0;
	static int s_need_wait = 1;

	sp_asoc_pr_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* NOTES: reduce linein pop noise must open ADCL/R ->HP/SPK MIXER
		   AFTER delay 250ms for both ADCL/ADCR switch complete.
		 */
		if (s_need_wait == 1) {
			/* NOTES: reduce linein pop noise must delay 250ms
			   after linein mixer switch on.
			   actually this function perform after
			   adc_switch_event function for
			   both ADCL/ADCR switch complete.
			 */
			sprd_codec_wait(250);
			s_need_wait++;
			sp_asoc_pr_dbg("ADC Switch ON delay\n");
		}
		mixer = &(sprd_codec->mixer[id]);
		if (mixer->set) {
			mixer->set(codec, mixer->on);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s_need_wait = 1;
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int mic_bias_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(w->reg);
	int ret = 0;
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	struct regulator **regu;

	sp_asoc_pr_dbg("%s %s Event is %s\n", __func__,
		       mic_bias_name[id], get_event_name(event));

	switch (id) {
	case SPRD_CODEC_MIC_BIAS:
		regu = &sprd_codec->main_mic;
		break;
	case SPRD_CODEC_AUXMIC_BIAS:
		regu = &sprd_codec->aux_mic;
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	ret = sprd_codec_mic_bias_set(regu, on);

	return ret;
}

static int mixer_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(w->reg);
	struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
	int ret = 0;

	sp_asoc_pr_dbg("%s event is %s\n", sprd_codec_mixer_debug_str[id],
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mixer->on = 1;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mixer->on = 0;
		break;
	default:
		BUG();
		ret = -EINVAL;
	}
	if (ret >= 0)
		_mixer_setting_one(codec, id, mixer->on, 1);

	return ret;
}

static int mixer_get(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = wlist->widgets[0]->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(mc->reg);
	ucontrol->value.integer.value[0] = sprd_codec->mixer[id].on;
	return 0;
}

static int mixer_need_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol, bool need_set)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = wlist->widgets[0]->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(mc->reg);
	struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
	int ret = 0;

	if (mixer->on == ucontrol->value.integer.value[0])
		return 0;

	sp_asoc_pr_info("Set %s Switch %s\n", sprd_codec_mixer_debug_str[id],
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	/*notice the sequence */
	snd_soc_dapm_put_volsw(kcontrol, ucontrol);

	/*update reg: must be set after snd_soc_dapm_put_enum_double->change = snd_soc_test_bits(widget->codec, e->reg, mask, val); */
	mixer->on = ucontrol->value.integer.value[0];

	if (mixer->set && need_set)
		ret = mixer->set(codec, mixer->on);

	return ret;
}

static int mixer_set_mem(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	return mixer_need_set(kcontrol, ucontrol, 0);
}

static int mixer_set(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	return mixer_need_set(kcontrol, ucontrol, 1);
}

#define SPRD_CODEC_MIXER(xname, xreg)\
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, mixer_get, mixer_set)

/*Just for LINE IN path, mixer_set not really set mixer (ADCL/R -> HP/SPK L/R) here but
setting in ana_loop_event, just remeber state here*/
#define SPRD_CODEC_MIXER_NOSET(xname, xreg)\
		SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, mixer_get, mixer_set_mem)

/* ADCL Mixer */
static const struct snd_kcontrol_new adcl_mixer_controls[] = {
	SPRD_CODEC_MIXER("AILADCL Switch",
			 ID_FUN(SPRD_CODEC_AIL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("AIRADCL Switch",
			 ID_FUN(SPRD_CODEC_AIR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("MainMICADCL Switch",
			 ID_FUN(SPRD_CODEC_MAIN_MIC, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("AuxMICADCL Switch",
			 ID_FUN(SPRD_CODEC_AUX_MIC, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("HPMICADCL Switch",
			 ID_FUN(SPRD_CODEC_HP_MIC, SPRD_CODEC_LEFT)),
};

/* ADCR Mixer */
static const struct snd_kcontrol_new adcr_mixer_controls[] = {
	SPRD_CODEC_MIXER("AILADCR Switch",
			 ID_FUN(SPRD_CODEC_AIL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("AIRADCR Switch",
			 ID_FUN(SPRD_CODEC_AIR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("MainMICADCR Switch",
			 ID_FUN(SPRD_CODEC_MAIN_MIC, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("AuxMICADCR Switch",
			 ID_FUN(SPRD_CODEC_AUX_MIC, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("HPMICADCR Switch",
			 ID_FUN(SPRD_CODEC_HP_MIC, SPRD_CODEC_RIGHT)),
};

/* HPL Mixer */
static const struct snd_kcontrol_new hpl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_DACL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("DACRHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_DACR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCLHPL Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCRHPL Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_LEFT)),
};

/* HPR Mixer */
static const struct snd_kcontrol_new hpr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_DACL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("DACRHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_DACR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCLHPR Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCRHPR Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_RIGHT)),
};

/* SPKL Mixer */
static const struct snd_kcontrol_new spkl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("DACRSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCLSPKL Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCRSPKL Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_LEFT)),
};

/* SPKR Mixer */
static const struct snd_kcontrol_new spkr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("DACRSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCLSPKR Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCRSPKR Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_RIGHT)),
};

/*ANA LOOP SWITCH*/
#define SPRD_CODEC_LOOP_SWITCH(xname, xreg)\
	SND_SOC_DAPM_PGA_S(xname, 7, FUN_REG(xreg), 0, 0, ana_loop_event,\
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD)

#ifndef SND_SOC_DAPM_MICBIAS_E
#define SND_SOC_DAPM_MICBIAS_E(wname, wreg, wshift, winvert, wevent, wflags) \
{	.id = snd_soc_dapm_micbias, .name = wname, .reg = wreg, .shift = wshift, \
	.invert = winvert, .kcontrol_news = NULL, .num_kcontrols = 0, \
	.event = wevent, .event_flags = wflags}
#endif

static const struct snd_soc_dapm_widget sprd_codec_dapm_widgets[] = {
	SND_SOC_DAPM_REGULATOR_SUPPLY("VBO", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("VCOM_BUF", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("Digital Power", 1, SND_SOC_NOPM, 0, 0,
			      digital_power_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("Analog Power", 2, SND_SOC_NOPM, 0, 0,
			      analog_power_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DA Clk", 3, SOC_REG(CCR), DAC_CLK_EN, 0, NULL,
			      0),
	SND_SOC_DAPM_SUPPLY_S("DRV Clk", 4, SOC_REG(CCR), DRV_CLK_EN, 0, NULL,
			      0),
	SND_SOC_DAPM_SUPPLY_S("AD IBUF", 3, SOC_REG(AACR1), ADC_IBUF_PD,
			      1,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AD Clk", 4, SOC_REG(CCR), ADC_CLK_PD, 1, NULL,
			      0),

	SND_SOC_DAPM_PGA_S("Digital DACL Switch", 4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_L, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital DACR Switch", 4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_R, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACL Switch", 5, SND_SOC_NOPM, 0, 0,
			   adie_dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADie Digital DACR Switch", 5, SND_SOC_NOPM, 0, 0,
			   adie_dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("DACL Switch", 6, SOC_REG(DACR), DACL_EN, 0, NULL,
			   0),
	SND_SOC_DAPM_PGA_S("DACR Switch", 6, SOC_REG(DACR), DACR_EN, 0, NULL,
			   0),
	SND_SOC_DAPM_DAC_E("DAC", "Playback-Vaudio",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0,
			   0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DFM-OUT", "DFM-Output",
			   SND_SOC_NOPM, 0,
			   0,
			   dfm_out_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
#ifdef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
	SND_SOC_DAPM_PGA_S("HP POP", 9, SND_SOC_NOPM, 0, 0, hp_pop_event,
			   SND_SOC_DAPM_POST_PMU),
#else
	SND_SOC_DAPM_SUPPLY_S("HP POP", 5, SND_SOC_NOPM, 0, 0, hp_pop_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
#endif
	SND_SOC_DAPM_PGA_S("HPL Switch", 5, SOC_REG(DCR1), HPL_EN, 0,
			   hp_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR Switch", 5, SOC_REG(DCR1), HPR_EN, 0,
			   hp_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPL Mute", 6, FUN_REG(SPRD_CODEC_PGA_HPL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR Mute", 6, FUN_REG(SPRD_CODEC_PGA_HPR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("HPL Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_mixer_controls[0],
			   ARRAY_SIZE(hpl_mixer_controls)),
	SND_SOC_DAPM_MIXER("HPR Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_mixer_controls[0],
			   ARRAY_SIZE(hpr_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPKL Mixer", SND_SOC_NOPM, 0, 0,
			   &spkl_mixer_controls[0],
			   ARRAY_SIZE(spkl_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPKR Mixer", SND_SOC_NOPM, 0, 0,
			   &spkr_mixer_controls[0],
			   ARRAY_SIZE(spkr_mixer_controls)),
	SPRD_CODEC_LOOP_SWITCH("ADCLHPL Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRHPL Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCLHPR Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRHPR Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_LOOP_SWITCH("ADCLSPKL Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRSPKL Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCLSPKR Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRSPKR Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_RIGHT)),
	SND_SOC_DAPM_PGA_S("SPKL Switch", 5, SOC_REG(DCR1), AOL_EN, 0,
			   spk_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("SPKR Switch", 5, SOC_REG(DCR1), AOR_EN, 0,
			   spk_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SPKL Mute", 6, FUN_REG(SPRD_CODEC_PGA_SPKL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("SPKR Mute", 6, FUN_REG(SPRD_CODEC_PGA_SPKR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("EAR Mixer", 5,
			   FUN_REG(ID_FUN
				   (SPRD_CODEC_EAR_DACL, SPRD_CODEC_LEFT)), 0,
			   0, mixer_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
#ifdef CONFIG_SND_SOC_SPRD_CODEC_EAR_WITH_IN_SPK
	SND_SOC_DAPM_PGA_S("EAR Switch", 6, SOC_REG(DCR1), EAR_EN, 0,
			   ear_switch_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
#else
	SND_SOC_DAPM_PGA_S("EAR Switch", 6, SOC_REG(DCR1), EAR_EN, 0, NULL, 0),
#endif

	SND_SOC_DAPM_PGA_S("EAR Mute", 7, FUN_REG(SPRD_CODEC_PGA_EAR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("ADCL Mixer", SND_SOC_NOPM, 0, 0,
			   &adcl_mixer_controls[0],
			   ARRAY_SIZE(adcl_mixer_controls)),
	SND_SOC_DAPM_MIXER("ADCR Mixer", SND_SOC_NOPM, 0, 0,
			   &adcr_mixer_controls[0],
			   ARRAY_SIZE(adcr_mixer_controls)),
	SND_SOC_DAPM_PGA_S("Digital ADCL Switch", 3, SOC_REG(AUD_TOP_CTL),
			   ADC_EN_L, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital ADCR Switch", 3, SOC_REG(AUD_TOP_CTL),
			   ADC_EN_R, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCL Switch", 2, SND_SOC_NOPM, 0, 0,
			   adie_adc_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCR Switch", 2, SND_SOC_NOPM, 0, 0,
			   adie_adc_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADCL Switch", 1, SOC_REG(AACR1), ADCL_PD, 1, NULL,
			   0),
	SND_SOC_DAPM_PGA_S("ADCR Switch", 1, SOC_REG(AACR1), ADCR_PD, 1, NULL,
			   0),
	SND_SOC_DAPM_PGA_E("ADCL PGA", SOC_REG(AACR2), ADCPGAL_PD, 1, NULL, 0,
			   adc_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ADCR PGA", SOC_REG(AACR2), ADCPGAR_PD, 1, NULL, 0,
			   adc_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADCL Mute", 3, FUN_REG(SPRD_CODEC_PGA_ADCL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("ADCR Mute", 3, FUN_REG(SPRD_CODEC_PGA_ADCR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "Main-Capture-Vaudio",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E("Mic Bias", FUN_REG(SPRD_CODEC_MIC_BIAS), 0, 0,
			       mic_bias_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MICBIAS_E("AuxMic Bias", FUN_REG(SPRD_CODEC_AUXMIC_BIAS),
			       0, 0,
			       mic_bias_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_S("Spk PA Switch", 15, SND_SOC_NOPM, 0, 0,
			   spk_pa_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("HEAD_P_L"),
	SND_SOC_DAPM_OUTPUT("HEAD_P_R"),
	SND_SOC_DAPM_OUTPUT("AOL"),
	SND_SOC_DAPM_OUTPUT("AOR"),
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_OUTPUT("HP PA"),
	SND_SOC_DAPM_OUTPUT("Spk PA"),
	SND_SOC_DAPM_OUTPUT("Spk2 PA"),

	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_INPUT("AUXMIC"),
	SND_SOC_DAPM_INPUT("HPMIC"),
	SND_SOC_DAPM_INPUT("AIL"),
	SND_SOC_DAPM_INPUT("AIR"),
};

/* sprd_codec supported interconnection*/
static const struct snd_soc_dapm_route sprd_codec_intercon[] = {
	/* Power */
	{"Analog Power", NULL, "VCOM_BUF"},
	{"Analog Power", NULL, "VBO"},
	{"DA Clk", NULL, "Analog Power"},
	{"DA Clk", NULL, "Digital Power"},
	{"DAC", NULL, "DA Clk"},
	{"DFM-OUT", NULL, "DA Clk"},

	{"AD IBUF", NULL, "Analog Power"},
	{"AD Clk", NULL, "Digital Power"},
	{"AD Clk", NULL, "AD IBUF"},
	{"ADC", NULL, "AD Clk"},

	{"ADCL PGA", NULL, "AD IBUF"},
	{"ADCR PGA", NULL, "AD IBUF"},

	{"HP POP", NULL, "DRV Clk"},
	{"HPL Switch", NULL, "DRV Clk"},
	{"HPR Switch", NULL, "DRV Clk"},
	{"SPKL Switch", NULL, "DRV Clk"},
	{"SPKR Switch", NULL, "DRV Clk"},
	{"EAR Switch", NULL, "DRV Clk"},

	/* Playback */
	{"Digital DACL Switch", NULL, "DFM-OUT"},
	{"Digital DACR Switch", NULL, "DFM-OUT"},
	{"Digital DACL Switch", NULL, "DAC"},
	{"Digital DACR Switch", NULL, "DAC"},
	{"ADie Digital DACL Switch", NULL, "Digital DACL Switch"},
	{"ADie Digital DACR Switch", NULL, "Digital DACR Switch"},
	{"DACL Switch", NULL, "ADie Digital DACL Switch"},
	{"DACR Switch", NULL, "ADie Digital DACR Switch"},

	/* Output */
	{"HPL Mixer", "DACLHPL Switch", "DACL Switch"},
	{"HPL Mixer", "DACRHPL Switch", "DACR Switch"},
	{"HPR Mixer", "DACLHPR Switch", "DACL Switch"},
	{"HPR Mixer", "DACRHPR Switch", "DACR Switch"},

	{"ADCLHPL Loop Switch", NULL, "ADCL PGA"},
	{"ADCLHPR Loop Switch", NULL, "ADCL PGA"},
	{"ADCRHPL Loop Switch", NULL, "ADCR PGA"},
	{"ADCRHPR Loop Switch", NULL, "ADCR PGA"},

	{"HPL Mixer", "ADCLHPL Switch", "ADCLHPL Loop Switch"},
	{"HPL Mixer", "ADCRHPL Switch", "ADCRHPL Loop Switch"},
	{"HPR Mixer", "ADCLHPR Switch", "ADCLHPR Loop Switch"},
	{"HPR Mixer", "ADCRHPR Switch", "ADCRHPR Loop Switch"},

#ifdef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
	{"HEAD_P_L", NULL, "HP POP"},
	{"HEAD_P_R", NULL, "HP POP"},
	{"HP POP", NULL, "HPL Switch"},
	{"HP POP", NULL, "HPR Switch"},
#else
	{"HPL Switch", NULL, "HP POP"},
	{"HPR Switch", NULL, "HP POP"},
#endif
	{"HPL Switch", NULL, "HPL Mixer"},
	{"HPR Switch", NULL, "HPR Mixer"},
	{"HPL Mute", NULL, "HPL Switch"},
	{"HPR Mute", NULL, "HPR Switch"},
	{"HEAD_P_L", NULL, "HPL Mute"},
	{"HEAD_P_R", NULL, "HPR Mute"},

	{"SPKL Mixer", "DACLSPKL Switch", "DACL Switch"},
	{"SPKL Mixer", "DACRSPKL Switch", "DACR Switch"},
	{"SPKR Mixer", "DACLSPKR Switch", "DACL Switch"},
	{"SPKR Mixer", "DACRSPKR Switch", "DACR Switch"},

	{"ADCLSPKL Loop Switch", NULL, "ADCL PGA"},
	{"ADCLSPKR Loop Switch", NULL, "ADCL PGA"},
	{"ADCRSPKL Loop Switch", NULL, "ADCR PGA"},
	{"ADCRSPKR Loop Switch", NULL, "ADCR PGA"},

	{"SPKL Mixer", "ADCLSPKL Switch", "ADCLSPKL Loop Switch"},
	{"SPKL Mixer", "ADCRSPKL Switch", "ADCRSPKL Loop Switch"},
	{"SPKR Mixer", "ADCLSPKR Switch", "ADCLSPKR Loop Switch"},
	{"SPKR Mixer", "ADCRSPKR Switch", "ADCRSPKR Loop Switch"},

	{"SPKL Switch", NULL, "SPKL Mixer"},
	{"SPKR Switch", NULL, "SPKR Mixer"},
	{"SPKL Mute", NULL, "SPKL Switch"},
	{"SPKR Mute", NULL, "SPKR Switch"},
	{"AOL", NULL, "SPKL Mute"},
	{"AOR", NULL, "SPKR Mute"},

	{"Spk PA", NULL, "Spk PA Switch"},
	{"Spk PA Switch", NULL, "SPKL Mute"},

	{"EAR Mixer", NULL, "DACL Switch"},
	{"EAR Switch", NULL, "EAR Mixer"},
	{"EAR Mute", NULL, "EAR Switch"},
	{"EAR", NULL, "EAR Mute"},

	{"ADCL Mute", NULL, "ADCL Mixer"},
	{"ADCR Mute", NULL, "ADCR Mixer"},
	{"ADCL PGA", NULL, "ADCL Mute"},
	{"ADCR PGA", NULL, "ADCR Mute"},
	/* LineIN */
	{"ADCL Mixer", "AILADCL Switch", "AIL"},
	{"ADCR Mixer", "AILADCR Switch", "AIL"},
	{"ADCL Mixer", "AIRADCL Switch", "AIR"},
	{"ADCR Mixer", "AIRADCR Switch", "AIR"},
	/* Input */
	{"ADCL Mixer", "MainMICADCL Switch", "Mic Bias"},
	{"ADCR Mixer", "MainMICADCR Switch", "Mic Bias"},
	{"ADCL Mixer", "AuxMICADCL Switch", "AuxMic Bias"},
	{"ADCR Mixer", "AuxMICADCR Switch", "AuxMic Bias"},
	{"ADCL Mixer", "HPMICADCL Switch", "HPMIC"},
	{"ADCR Mixer", "HPMICADCR Switch", "HPMIC"},
	/* Captrue */
	{"ADCL Switch", NULL, "ADCL PGA"},
	{"ADCR Switch", NULL, "ADCR PGA"},
	{"ADie Digital ADCL Switch", NULL, "ADCL Switch"},
	{"ADie Digital ADCR Switch", NULL, "ADCR Switch"},
	{"Digital ADCL Switch", NULL, "ADie Digital ADCL Switch"},
	{"Digital ADCR Switch", NULL, "ADie Digital ADCR Switch"},
	{"ADC", NULL, "Digital ADCL Switch"},
	{"ADC", NULL, "Digital ADCR Switch"},

	{"Mic Bias", NULL, "MIC"},
	{"AuxMic Bias", NULL, "AUXMIC"},
	/* Bias independent */
	{"Mic Bias", NULL, "Analog Power"},
	{"AuxMic Bias", NULL, "Analog Power"},
};

static int sprd_codec_vol_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[reg]);
	int ret = 0;

	sp_asoc_pr_info("Set PGA[%s] to %ld\n", sprd_codec_pga_debug_str[reg],
			ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	pga->pgaval = val;
	if (pga->set) {
		ret = pga->set(codec, pga->pgaval);
	}
	return ret;
}

static int sprd_codec_vol_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int invert = mc->invert;
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[reg]);

	ucontrol->value.integer.value[0] = pga->pgaval;
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
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	int ret = 0;

	sp_asoc_pr_info("Config inter PA 0x%08x\n",
			(int)ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mutex_lock(&sprd_codec->inter_pa_mutex);
	sprd_codec->inter_pa.value = (u32) val;
	if (sprd_codec->inter_pa.set) {
		mutex_unlock(&sprd_codec->inter_pa_mutex);
		sprd_inter_speaker_pa(codec, 1);
	} else {
		mutex_unlock(&sprd_codec->inter_pa_mutex);
	}
	return ret;
}

static int sprd_codec_inter_pa_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int invert = mc->invert;

	mutex_lock(&sprd_codec->inter_pa_mutex);
	ucontrol->value.integer.value[0] = sprd_codec->inter_pa.value;
	mutex_unlock(&sprd_codec->inter_pa_mutex);
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_mic_bias_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	int ret = 0;
	int id = reg;

	val = (ucontrol->value.integer.value[0] & mask);

	if (invert)
		val = max - val;
	if (sprd_codec->mic_bias[id] == val) {
		return 0;
	}

	sp_asoc_pr_info("%s Switch %s\n", mic_bias_name[id], STR_ON_OFF(val));

	sprd_codec->mic_bias[id] = val;
	if (val) {
		ret =
		    snd_soc_dapm_force_enable_pin(&codec->card->dapm,
						  mic_bias_name[id]);
	} else {
		ret =
		    snd_soc_dapm_disable_pin(&codec->card->dapm,
					     mic_bias_name[id]);
	}

	/* signal a DAPM event */
	snd_soc_dapm_sync(&codec->card->dapm);

	return ret;
}

static int sprd_codec_mic_bias_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int invert = mc->invert;
	int id = reg;

	ucontrol->value.integer.value[0] = sprd_codec->mic_bias[id];
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(adc_tlv, -600, 300, 0);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -3600, 300, 1);
static const DECLARE_TLV_DB_SCALE(ear_tlv, -3600, 300, 1);
static const DECLARE_TLV_DB_SCALE(spk_tlv, -2400, 300, 1);

#define SPRD_CODEC_PGA(xname, xreg, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, FUN_REG(xreg), 0, 15, 0, \
			sprd_codec_vol_get, sprd_codec_vol_put, tlv_array)

#define SPRD_CODEC_MIC_BIAS(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, \
			sprd_codec_mic_bias_get, sprd_codec_mic_bias_put)

static const struct snd_kcontrol_new sprd_codec_snd_controls[] = {
	SPRD_CODEC_PGA("SPKL Playback Volume", SPRD_CODEC_PGA_SPKL, spk_tlv),
	SPRD_CODEC_PGA("SPKR Playback Volume", SPRD_CODEC_PGA_SPKR, spk_tlv),
	SPRD_CODEC_PGA("HPL Playback Volume", SPRD_CODEC_PGA_HPL, hp_tlv),
	SPRD_CODEC_PGA("HPR Playback Volume", SPRD_CODEC_PGA_HPR, hp_tlv),
	SPRD_CODEC_PGA("EAR Playback Volume", SPRD_CODEC_PGA_EAR, ear_tlv),

	SPRD_CODEC_PGA("ADCL Capture Volume", SPRD_CODEC_PGA_ADCL, adc_tlv),
	SPRD_CODEC_PGA("ADCR Capture Volume", SPRD_CODEC_PGA_ADCR, adc_tlv),

	SOC_SINGLE_EXT("Inter PA Config", 0, 0, LONG_MAX, 0,
		       sprd_codec_inter_pa_get, sprd_codec_inter_pa_put),

	SPRD_CODEC_MIC_BIAS("MIC Bias Switch", SPRD_CODEC_MIC_BIAS),

	SPRD_CODEC_MIC_BIAS("AUXMIC Bias Switch", SPRD_CODEC_AUXMIC_BIAS),
};

static unsigned int sprd_codec_read(struct snd_soc_codec *codec,
				    unsigned int reg)
{
	/* Because snd_soc_update_bits reg is 16 bits short type,
	   so muse do following convert
	 */
	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		return arch_audio_codec_read(reg);
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;
		return __raw_readl((void __iomem *)reg);
	} else if (IS_SPRD_CODEC_MIXER_RANG(FUN_REG(reg))) {
		struct sprd_codec_priv *sprd_codec =
		    snd_soc_codec_get_drvdata(codec);
		int id = FUN_REG(reg);
		struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
		return mixer->on;
	}

	sp_asoc_pr_dbg("read the register is not codec's reg = 0x%x\n", reg);
	return 0;
}

static int sprd_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			    unsigned int val)
{
	int ret = 0;
	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		ret = arch_audio_codec_write(reg, val);
		sp_asoc_pr_reg("A[0x%04x] W:[0x%08x] R:[0x%08x]\n",
			       reg & 0xFFFF, val, arch_audio_codec_read(reg));
		return ret;
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;
		__raw_writel(val, (void __iomem *)reg);
		sp_asoc_pr_reg("D[0x%04x] W:[0x%08x] R:[0x%08x]\n",
			       reg & 0xFFFF, val,
			       __raw_readl((void __iomem *)reg));
		return ret;
	}
	sp_asoc_pr_dbg("write the register is not codec's reg = 0x%x\n", reg);
	return ret;
}

static int sprd_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int mask = 0x0F;
	int shift = 0;
	int rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sprd_codec->da_sample_val = rate;
		sp_asoc_pr_info("Playback rate is [%d]\n", rate);
		sprd_codec_set_sample_rate(codec, rate, mask, shift);
	} else {
		sprd_codec->ad_sample_val = rate;
		sp_asoc_pr_info("Capture rate is [%d]\n", rate);
		sprd_codec_set_ad_sample_rate(codec, rate, mask, shift);
	}

	return 0;
}

static int sprd_codec_pcm_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	return 0;
}

#if 0
static void sprd_codec_dac_mute_irq_enable(struct snd_soc_codec *codec)
{
	int mask = BIT(DAC_MUTE_D);
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_CLR), mask, mask);
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_EN), mask, mask);
}
#endif

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
	if (BIT(DAC_MUTE_U_MASK) & mask) {
		mask = BIT(DAC_MUTE_U);
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_EN), mask, 0);
	return IRQ_HANDLED;
}

#if 0
static int sprd_codec_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_dbg("%s\n", __func__);

	if (atomic_read(&sprd_codec->ldo_refcount) >= 1) {
		sp_asoc_pr_dbg("mute %i\n", mute);

		ret =
		    snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL),
					BIT(DAC_MUTE_START),
					mute ? BIT(DAC_MUTE_START) : 0);
#if 0
		if (mute && ret) {
			int dac_mute_complete;
			struct sprd_codec_priv *sprd_codec =
			    snd_soc_codec_get_drvdata(codec);
			sp_asoc_pr_dbg("Dac Mute IRQ enable\n");
			sprd_codec_dac_mute_irq_enable(codec);
			init_completion(&sprd_codec->completion_dac_mute);
			dac_mute_complete =
			    wait_for_completion_timeout
			    (&sprd_codec->completion_dac_mute,
			     msecs_to_jiffies(SPRD_CODEC_DAC_MUTE_TIMEOUT));
			sp_asoc_pr_dbg("Dac Mute Completion %d\n",
				       dac_mute_complete);
			if (!dac_mute_complete) {
				pr_err("Dac Mute Timeout\n");
			}
		}
#endif

	}

	return ret;
}
#endif

static struct snd_soc_dai_ops sprd_codec_dai_ops = {
	.hw_params = sprd_codec_pcm_hw_params,
	.hw_free = sprd_codec_pcm_hw_free,
};

#ifdef CONFIG_PM
static int sprd_codec_soc_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int sprd_codec_soc_resume(struct snd_soc_codec *codec)
{
	return 0;
}
#else
#define sprd_codec_soc_suspend NULL
#define sprd_codec_soc_resume  NULL
#endif

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
static void sprd_codec_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;
	struct snd_soc_codec *codec = sprd_codec->codec;
	int reg;

	snd_iprintf(buffer, "%s digital part\n", codec->name);
	for (reg = SPRD_CODEC_DP_BASE; reg < SPRD_CODEC_DP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - SPRD_CODEC_DP_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		    );
	}
	snd_iprintf(buffer, "%s analog part\n", codec->name);
	for (reg = SPRD_CODEC_AP_BASE; reg < SPRD_CODEC_AP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - SPRD_CODEC_AP_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		    );
	}
}

static void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
	struct snd_info_entry *entry;
	struct snd_soc_codec *codec = sprd_codec->codec;

	if (!snd_card_proc_new(codec->card->snd_card, "sprd-codec", &entry))
		snd_info_set_text_ops(entry, sprd_codec, sprd_codec_proc_read);
}
#else /* !CONFIG_PROC_FS */
static inline void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
}
#endif

static void sprd_codec_power_changed(struct power_supply *psy)
{
	struct sprd_codec_priv *sprd_codec = 0;
	sp_asoc_pr_dbg("%s %s\n", __func__, psy->name);
#if 1
	sprd_codec = container_of(psy, struct sprd_codec_priv, audio_ldo);
	mutex_lock(&sprd_codec->inter_pa_mutex);
	if (sprd_codec->inter_pa.set && sprd_codec->inter_pa.setting.is_LDO_mode
	    && sprd_codec->inter_pa.setting.is_auto_LDO_mode) {
		sprd_codec_pa_ldo_auto(sprd_codec->codec, 0);
	}
	mutex_unlock(&sprd_codec->inter_pa_mutex);
	if (atomic_read(&sprd_codec->ldo_refcount) >= 1) {
		sprd_codec_vcom_ldo_auto(sprd_codec->codec, 0);
	}
#endif
}

static int sprd_codec_power_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	return -EINVAL;
}

static int sprd_codec_audio_ldo(struct sprd_codec_priv *sprd_codec)
{
	int ret = 0;
	struct snd_soc_codec *codec = sprd_codec->codec;
	sprd_codec->audio_ldo.name = "audio-ldo";
	sprd_codec->audio_ldo.get_property = sprd_codec_power_get_property;
	sprd_codec->audio_ldo.external_power_changed = sprd_codec_power_changed;
	ret = power_supply_register(codec->dev, &sprd_codec->audio_ldo);
	if (ret) {
		pr_err("ERR:Register power supply error!\n");
		return -EFAULT;
	}
	return ret;
}

#define SPRD_CODEC_PCM_RATES 	\
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_11025 | \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_22050 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_96000)

#define SPRD_CODEC_PCM_AD_RATES	\
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_48000)

/* PCM Playing and Recording default in full duplex mode */
static struct snd_soc_dai_driver sprd_codec_dai[] = {
	{
	 .name = "sprd-codec-v1-i2s",
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SPRD_CODEC_PCM_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .stream_name = "Main-Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SPRD_CODEC_PCM_AD_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &sprd_codec_dai_ops,
	 },
#ifdef CONFIG_SND_SOC_SPRD_VAUDIO
	{
	 .name = "sprd-codec-v1-vaudio",
	 .playback = {
		      .stream_name = "Playback-Vaudio",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SPRD_CODEC_PCM_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .stream_name = "Main-Capture-Vaudio",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SPRD_CODEC_PCM_AD_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &sprd_codec_dai_ops,
	 },
#endif
	{
	 .name = "sprd-codec-v1-fm",
	 .playback = {
		      .stream_name = "DFM-Output",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SPRD_CODEC_PCM_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .ops = &sprd_codec_dai_ops,
	 },
};

static int sprd_codec_soc_probe(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_dbg("%s\n", __func__);

	codec->dapm.idle_bias_off = 1;

	sprd_codec->codec = codec;

	sprd_codec_proc_init(sprd_codec);

	sprd_codec_audio_ldo(sprd_codec);

	return ret;
}

/* power down chip */
static int sprd_codec_soc_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int sprd_codec_power_regulator_init(struct sprd_codec_priv *sprd_codec,
					   struct device *dev)
{
	sprd_codec_power_get(dev, &sprd_codec->main_mic, "MICBIAS");
	sprd_codec_power_get(dev, &sprd_codec->aux_mic, "AUXMICBIAS");
	return 0;
}

static void sprd_codec_power_regulator_exit(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec_power_put(&sprd_codec->main_mic);
	sprd_codec_power_put(&sprd_codec->aux_mic);
}

static struct snd_soc_codec_driver soc_codec_dev_sprd_codec = {
	.probe = sprd_codec_soc_probe,
	.remove = sprd_codec_soc_remove,
	.suspend = sprd_codec_soc_suspend,
	.resume = sprd_codec_soc_resume,
	.read = sprd_codec_read,
	.write = sprd_codec_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.dapm_widgets = sprd_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
	.dapm_routes = sprd_codec_intercon,
	.num_dapm_routes = ARRAY_SIZE(sprd_codec_intercon),
	.controls = sprd_codec_snd_controls,
	.num_controls = ARRAY_SIZE(sprd_codec_snd_controls),
};

static int sprd_codec_probe(struct platform_device *pdev)
{
	struct sprd_codec_priv *sprd_codec;
	int ret;
	struct device_node *node = pdev->dev.of_node;

	sp_asoc_pr_dbg("%s\n", __func__);

	arch_audio_codec_switch(AUDIO_TO_ARM_CTRL);

	sprd_codec =
	    devm_kzalloc(&pdev->dev, sizeof(struct sprd_codec_priv),
			 GFP_KERNEL);
	if (sprd_codec == NULL)
		return -ENOMEM;

	sprd_codec->da_sample_val = 44100;	/*inital value for FM route */

	if (node) {
		u32 val;
		if (!of_property_read_u32(node, "sprd,ap_irq", &val)) {
			sprd_codec->ap_irq = val;
			sp_asoc_pr_dbg("Set AP IRQ is %d!\n", val);
		} else {
			pr_err("ERR:Must give me the AP IRQ!\n");
			return -EINVAL;
		}
		if (!of_property_read_u32(node, "sprd,dp_irq", &val)) {
			sprd_codec->dp_irq = val;
			sp_asoc_pr_dbg("Set DP IRQ is %d!\n", val);
		} else {
			pr_err("ERR:Must give me the DP IRQ!\n");
			return -EINVAL;
		}
		if (!of_property_read_u32(node, "sprd,def_da_fs", &val)) {
			sprd_codec->da_sample_val = val;
			sp_asoc_pr_dbg("Change DA default fs to %d!\n", val);
		}
	} else {
		sprd_codec->ap_irq = CODEC_AP_IRQ;
		sprd_codec->dp_irq = CODEC_DP_IRQ;
	}

	platform_set_drvdata(pdev, sprd_codec);

	atomic_set(&sprd_codec->adie_adc_refcount, 0);
	atomic_set(&sprd_codec->adie_dac_refcount, 0);

	ret =
	    request_irq(sprd_codec->ap_irq, sprd_codec_ap_irq, 0,
			"sprd_codec_ap", sprd_codec);
	if (ret) {
		pr_err("ERR:Request irq ap failed!\n");
		goto err_irq;
	}

	INIT_DELAYED_WORK(&sprd_codec->ovp_delayed_work,
			  sprd_codec_ovp_delay_worker);

	ret =
	    request_irq(sprd_codec->dp_irq, sprd_codec_dp_irq, 0,
			"sprd_codec_dp", sprd_codec);
	if (ret) {
		pr_err("ERR:Request irq dp failed!\n");
		goto dp_err_irq;
	}

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_sprd_codec,
				     sprd_codec_dai,
				     ARRAY_SIZE(sprd_codec_dai));
	if (ret != 0) {
		pr_err("ERR:Failed to register CODEC: %d\n", ret);
		return ret;
	}

	sprd_codec_inter_pa_init(sprd_codec);

	sprd_codec_power_regulator_init(sprd_codec, &pdev->dev);

	mutex_init(&sprd_codec->inter_pa_mutex);
	spin_lock_init(&sprd_codec->sprd_codec_fun_lock);
	spin_lock_init(&sprd_codec->sprd_codec_pa_sw_lock);

	/* SC8825C need change the v_map */
	sprd_codec_vcom_ldo_cfg(sprd_codec, ldo_vcom_v_map,
				ARRAY_SIZE(ldo_vcom_v_map));
	sprd_codec_pa_ldo_cfg(sprd_codec, ldo_v_map, ARRAY_SIZE(ldo_v_map));

	return 0;

dp_err_irq:
	free_irq(sprd_codec->ap_irq, sprd_codec);
err_irq:
	return -EINVAL;
}

static int sprd_codec_remove(struct platform_device *pdev)
{
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);
	sprd_codec_power_regulator_exit(sprd_codec);
	free_irq(sprd_codec->ap_irq, sprd_codec);
	free_irq(sprd_codec->dp_irq, sprd_codec);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id codec_of_match[] = {
	{.compatible = "sprd,sprd-codec-v1",},
	{},
};

MODULE_DEVICE_TABLE(of, codec_of_match);
#endif

static struct platform_driver sprd_codec_codec_driver = {
	.driver = {
		   .name = "sprd-codec-v1",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(codec_of_match),
		   },
	.probe = sprd_codec_probe,
	.remove = sprd_codec_remove,
};

module_platform_driver(sprd_codec_codec_driver);

MODULE_DESCRIPTION("SPRD-CODEC ALSA SoC codec driver");
MODULE_AUTHOR("Zhenfang Wang <zhenfang.wang@spreadtrum.com>");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:sprd-codec");
