/*
 * sound/soc/sprd/codec/sprd/v4/sprd-codec.c
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
#define pr_fmt(fmt) pr_sprd_fmt("CDCV4") fmt

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
#include <mach/arch_misc.h>

#ifdef CONFIG_SND_SOC_VBC_SRC_SAMPLE_RATE
#if  (CONFIG_SND_SOC_VBC_SRC_SAMPLE_RATE != 32000) && (CONFIG_SND_SOC_CODEC_SRC_SAMPLE_RATE != 48000)
#error "We hope the CODEC sample rate is 32KHz or 48KHz!"
#endif
#define SRC_SUPPORT_RATE (SNDRV_PCM_RATE_44100)
#else /* !CONFIG_SND_SOC_VBC_SRC_SAMPLE_RATE */
#define SRC_SUPPORT_RATE (0)
#endif

int hp_register_notifier(struct notifier_block *nb);
int hp_unregister_notifier(struct notifier_block *nb);
int get_hp_plug_state(void);

#define SOC_REG(r) ((unsigned short)(r))
#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define ID_FUN(id, lr) ((unsigned short)(((id) << 1) | (lr)))

#define SPRD_CODEC_AP_BASE_HI (SPRD_CODEC_AP_BASE & 0xFFFF0000)
#define SPRD_CODEC_DP_BASE_HI (SPRD_CODEC_DP_BASE & 0xFFFF0000)

enum {
	SPRD_CODEC_HP_PA_ORDER = 100,
	SPRD_CODEC_HP_SWITCH_ORDER = 101,
	SPRD_CODEC_ANA_MIXER_ORDER = 102,  /*must be later with switch order*/
	SPRD_CODEC_HP_PA_POST_ORDER = 103,
	SPRD_CODEC_HP_POP_ORDER = 104,
	SPRD_CODEC_PA_ORDER = 104, /*last power up, later with ANA_MIXER order*/
	SPRD_CODEC_CG_PGA_ORDER
};

enum {
	SPRD_CODEC_AILADCL_SET = 0,
	SPRD_CODEC_AILADCR_SET,
	SPRD_CODEC_AIRADCL_SET,
	SPRD_CODEC_AIRADCR_SET,
	SPRD_CODEC_LINEIN_END,
};

enum {
	SPRD_CODEC_ADCLSPEAKER_L_SET = 0,
	SPRD_CODEC_ADCLSPEAKER_R_SET,
	SPRD_CODEC_ADCRSPEAKER_L_SET,
	SPRD_CODEC_ADCRSPEAKER_R_SET,
	SPRD_CODEC_ADCLHEADPHONE_L_SET,
	SPRD_CODEC_ADCLHEADPHONE_R_SET,
	SPRD_CODEC_ADCRHEADPHONE_L_SET,
	SPRD_CODEC_ADCRHEADPHONE_R_SET,
	SPRD_CODEC_ADC_END,
};


enum {
	SPRD_CODEC_PLAYBACK,
	SPRD_CODEC_CAPTRUE,
	SPRD_CODEC_CAPTRUE1,
	SPRD_CODEC_CHAN_MAX,
};

static const char *sprd_codec_chan_name[SPRD_CODEC_CHAN_MAX] = {
	"DAC",
	"ADC",
	"ADC1",
};

static inline const char *sprd_codec_chan_get_name(int chan_id)
{
	return sprd_codec_chan_name[chan_id];
}

enum {
	SPRD_CODEC_PGA_START = 0,
	SPRD_CODEC_PGA_SPKL = SPRD_CODEC_PGA_START,
	SPRD_CODEC_PGA_SPKR,
	SPRD_CODEC_PGA_HPL,
	SPRD_CODEC_PGA_HPR,
	SPRD_CODEC_PGA_EAR,
	SPRD_CODEC_PGA_ADCL,
	SPRD_CODEC_PGA_ADCR,
	SPRD_CODEC_PGA_DACL,
	SPRD_CODEC_PGA_DACR,
	SPRD_CODEC_PGA_MIC,
	SPRD_CODEC_PGA_AUXMIC,
	SPRD_CODEC_PGA_HEADMIC,
	SPRD_CODEC_PGA_AIL,
	SPRD_CODEC_PGA_AIR,
	SPRD_CODEC_PGA_CG_HPL_1,
	SPRD_CODEC_PGA_CG_HPR_1,
	SPRD_CODEC_PGA_CG_HPL_2,
	SPRD_CODEC_PGA_CG_HPR_2,

	SPRD_CODEC_PGA_END
};

#define GET_PGA_ID(x)   ((x)-SPRD_CODEC_PGA_START)
#define SPRD_CODEC_PGA_MAX  (SPRD_CODEC_PGA_END - SPRD_CODEC_PGA_START)
static const char *sprd_codec_pga_debug_str[SPRD_CODEC_PGA_MAX] = {
	"SPKL",
	"SPKR",
	"HPL",
	"HPR",
	"EAR",
	"ADCL",
	"ADCR",
	"DACL",
	"DACR",
	"MIC",
	"AUXMIC",
	"HEADMIC",
	"AIL",
	"AIR",
	"CG_HPL_1",
	"CG_HPR_1",
	"CG_HPL_2",
	"CG_HPR_2",
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
	SPRD_CODEC_MIXER_START = SPRD_CODEC_PGA_END + 20,
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

	SPRD_CODEC_MIXER_END
};

#define GET_MIXER_ID(x)    ((unsigned short)(x)-ID_FUN(SPRD_CODEC_MIXER_START, SPRD_CODEC_LEFT))
#define SPRD_CODEC_MIXER_MAX (ID_FUN(SPRD_CODEC_MIXER_END, SPRD_CODEC_LEFT) -ID_FUN(SPRD_CODEC_MIXER_START, SPRD_CODEC_LEFT))
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

#define IS_SPRD_CODEC_MIXER_RANG(reg) (((reg) >= ID_FUN(SPRD_CODEC_MIXER_START, SPRD_CODEC_LEFT)) && ((reg) <= ID_FUN(SPRD_CODEC_MIXER_END, SPRD_CODEC_LEFT)))
#define IS_SPRD_CODEC_PGA_RANG(reg) ((reg) <= SPRD_CODEC_PGA_END)
#define IS_SPRD_CODEC_MIC_BIAS_RANG(reg) (((reg) >= SPRD_CODEC_MIC_BIAS_START) && ((reg) <= SPRD_CODEC_MIC_BIAS_END))

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
	{LDO_V_28, 2800},
	{LDO_V_29, 2900},
	{LDO_V_30, 3000},
	{LDO_V_31, 3100},
	{LDO_V_32, 3200},
	{LDO_V_33, 3300},
	{LDO_V_34, 3400},
	{LDO_V_36, 3600},
};

struct sprd_codec_ldo_cfg {
	const struct sprd_codec_ldo_v_map *v_map;
	int v_map_size;
};

struct sprd_codec_inter_pa {
	/* FIXME little endian */
	u32 LDO_V_sel:4;
	u32 DTRI_F_sel:4;
	u32 is_DEMI_mode:1;
	u32 is_classD_mode:1;
	u32 is_LDO_mode:1;
	u32 is_auto_LDO_mode:1;
	u32 RESV:20;
};

struct sprd_codec_pa_setting {
	union {
		struct sprd_codec_inter_pa setting;
		u32 value;
	};
	int set;
};

struct sprd_codec_inter_hp_pa {
	/* FIXME little endian */
	u32 class_g_osc:2;
	u32 class_g_mode:2;
	u32 class_g_low_power:2;
	u32 class_g_cgcal:2;
	u32 class_g_pa_en_delay_10ms:4;
	u32 class_g_hp_switch_delay_10ms:4;
	u32 class_g_hp_on_delay_20ms:4;
/*
	u32 class_g_unmute_delay_100ms:3;
	u32 class_g_cgcal_delay_10ms:3;
	u32 class_g_close_delay_10ms:2;
	u32 class_g_open_delay_10ms:4;
	u32  class_g_vdd_delay_10ms:6;
	u32  class_g_chp_delay_30ms:4;
	u32  class_g_all_close_delay_100ms:2;
	//u32 RESV:0;
*/
};

struct sprd_codec_hp_pa_setting {
	union {
		struct sprd_codec_inter_hp_pa setting;
		u32 value;
	};
	int set;
};

enum {
	SPRD_CODEC_MIC_BIAS_START =
	    ID_FUN(SPRD_CODEC_MIXER_END, SPRD_CODEC_LEFT) + 100,
	SPRD_CODEC_MIC_BIAS = SPRD_CODEC_MIC_BIAS_START,
	SPRD_CODEC_AUXMIC_BIAS,
	SPRD_CODEC_HEADMIC_BIAS,

	SPRD_CODEC_MIC_BIAS_END
};

#define GET_MIC_BIAS_ID(x)   ((x)-SPRD_CODEC_MIC_BIAS_START)
#define SPRD_CODEC_MIC_BIAS_MAX (SPRD_CODEC_MIC_BIAS_END -SPRD_CODEC_MIC_BIAS_START)
static const char *mic_bias_name[SPRD_CODEC_MIC_BIAS_MAX] = {
	"Mic Bias",
	"AuxMic Bias",
	"HeadMic Bias",
};

enum {
	SPRD_CODEC_HP_PA_VER_1,
	SPRD_CODEC_HP_PA_VER_2,
	SPRD_CODEC_HP_PA_VER_MAX,
};

/* codec private data */
struct sprd_codec_priv {
	struct snd_soc_codec *codec;
	int da_sample_val;
	int ad_sample_val;
	int ad1_sample_val;
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

	int hp_ver;
	struct sprd_codec_hp_pa_setting inter_hp_pa;
	struct mutex inter_hp_pa_mutex;

	struct regulator *main_mic;
	struct regulator *aux_mic;
	struct regulator *head_mic;
	struct regulator *cg_ldo;
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

	int sprd_linein_mute;
	int sprd_linein_speaker_mute;
	int sprd_linein_headphone_mute;
	int sprd_linein_set;
	int sprd_linein_speaker_set;
	int sprd_linein_headphone_set;
	struct mutex sprd_linein_mute_mutex;
	struct mutex sprd_linein_speaker_mute_mutex;
	struct notifier_block nb;
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

static int sprd_codec_regulator_set(struct regulator **regu, int on)
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
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU2), mask, val);
}

static inline void sprd_codec_auxadc_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("AUXADC Set %d\n", on);
    /*jian.chen changed*/
    /*
	mask = BIT(AUXADC_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(PMUR4_PMUR3), mask, val);
	*/
}

static inline void sprd_codec_dodvl_set(struct snd_soc_codec *codec, int sval)
{
     int mask;
     int val;
     sp_asoc_pr_dbg("DAdodvl Set %d\n", sval);
     mask = DAC_SDM_DODVL_MASK << DAC_SDM_DODVL_START;
     val = (sval << DAC_SDM_DODVL_START) & mask;
     snd_soc_update_bits(codec, SOC_REG(AUD_SDM_CTL0), mask, val);
}

static int sprd_codec_pga_spk_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC11;
	mask = AOL_G_MASK << AOL_G;
	val = (pgaval << AOL_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_spkr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC11;
	mask = AOR_G_MASK << AOR_G;
	val = (pgaval << AOR_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_hpl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC12;
	mask = HPL_G_MASK << HPL_G;
	val = (pgaval << HPL_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_hpr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC12;
	mask = HPR_G_MASK << HPR_G;
	val = (pgaval << HPR_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_ear_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC12;
	mask = EAR_G_MASK << EAR_G;
	val = (pgaval << EAR_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_adcl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC9;
	mask = ADC_PGA_L_MASK << ADC_PGA_L;
	val = (pgaval << ADC_PGA_L) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_adcr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC9;
	mask = ADC_PGA_R_MASK << ADC_PGA_R;
	val = (pgaval << ADC_PGA_R) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_dacl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC10;
	mask = DACL_G_MASK << DACL_G;
	val = (pgaval << DACL_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_dacr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC10;
	mask = DACR_G_MASK << DACR_G;
	val = (pgaval << DACR_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_mic_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC8;
	mask = ADC_MIC_PGA_MASK << ADC_MIC_PGA;
	val = (pgaval << ADC_MIC_PGA) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_auxmic_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC8;
	mask = ADC_AUXMIC_PGA_MASK << ADC_AUXMIC_PGA;
	val = (pgaval << ADC_AUXMIC_PGA) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_headmic_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC8;
	mask = ADC_HEADMIC_PGA_MASK << ADC_HEADMIC_PGA;
	val = (pgaval << ADC_HEADMIC_PGA) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_ailr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC8;
	mask = ADC_AILR_PGA_MASK << ADC_AILR_PGA;
	val = (pgaval << ADC_AILR_PGA) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_cg_hpl_1_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC13;
	mask = CG_HPL_G_1_MASK << CG_HPL_G_1;
	val = (pgaval << CG_HPL_G_1) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_cg_hpr_1_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC14;
	mask = CG_HPR_G_1_MASK << CG_HPR_G_1;
	val = (pgaval << CG_HPR_G_1) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_cg_hpl_2_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC13;
	mask = CG_HPL_G_2_MASK << CG_HPL_G_2;
	val = (pgaval << CG_HPL_G_2) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_cg_hpr_2_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;
	reg = ANA_CDC14;
	mask = CG_HPR_G_2_MASK << CG_HPR_G_2;
	val = (pgaval << CG_HPR_G_2) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static struct sprd_codec_pga sprd_codec_pga_cfg[SPRD_CODEC_PGA_MAX] = {
	{sprd_codec_pga_spk_set, 0},
	{sprd_codec_pga_spkr_set, 0},
	{sprd_codec_pga_hpl_set, 0},
	{sprd_codec_pga_hpr_set, 0},
	{sprd_codec_pga_ear_set, 0},

	{sprd_codec_pga_adcl_set, 0},
	{sprd_codec_pga_adcr_set, 0},

	{sprd_codec_pga_dacl_set, 0},
	{sprd_codec_pga_dacr_set, 0},
	{sprd_codec_pga_mic_set, 0},
	{sprd_codec_pga_auxmic_set, 0},
	{sprd_codec_pga_headmic_set, 0},
	{sprd_codec_pga_ailr_set, 0},
	{sprd_codec_pga_ailr_set, 0},
	{sprd_codec_pga_cg_hpl_1_set, -1},
	{sprd_codec_pga_cg_hpr_1_set, -1},
	{sprd_codec_pga_cg_hpl_2_set, -1},
	{sprd_codec_pga_cg_hpr_2_set, -1},
};

static void _mixer_adc_linein_mute_nolock(struct snd_soc_codec *codec, int need_mute)
{
	int val = 0;
	val = !need_mute;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	if (sprd_codec->sprd_linein_set & BIT(SPRD_CODEC_AILADCL_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), BIT(AIL_ADCL), val << AIL_ADCL);
	}
	if (sprd_codec->sprd_linein_set & BIT(SPRD_CODEC_AILADCR_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), BIT(AIL_ADCR), val << AIL_ADCR);
	}
	if (sprd_codec->sprd_linein_set & BIT(SPRD_CODEC_AIRADCL_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), BIT(AIR_ADCL), val << AIR_ADCL);
	}
	if (sprd_codec->sprd_linein_set & BIT(SPRD_CODEC_AIRADCR_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), BIT(AIR_ADCR), val << AIR_ADCR);
	}
}

static void _mixer_linein_speaker_mute_nolock(struct snd_soc_codec *codec, int need_mute)
{
	int val = 0;
	val = !need_mute;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	if (sprd_codec->sprd_linein_speaker_set & BIT(SPRD_CODEC_ADCLSPEAKER_L_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(ADCL_AOL), val << ADCL_AOL);
	}
	if (sprd_codec->sprd_linein_speaker_set & BIT(SPRD_CODEC_ADCRSPEAKER_L_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(ADCR_AOL), val << ADCR_AOL);
	}
}

static void _mixer_linein_headphone_mute_nolock(struct snd_soc_codec *codec, int need_mute)
{
	int val = 0;
	val = !need_mute;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	if (sprd_codec->sprd_linein_headphone_set & BIT(SPRD_CODEC_ADCLHEADPHONE_L_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC7), BIT(ADCL_P_HPL), val << ADCL_P_HPL);
	}
	if (sprd_codec->sprd_linein_headphone_set & BIT(SPRD_CODEC_ADCLHEADPHONE_R_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC7), BIT(ADCL_N_HPR), val << ADCL_N_HPR);
	}
	if (sprd_codec->sprd_linein_headphone_set & BIT(SPRD_CODEC_ADCRHEADPHONE_L_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC7), BIT(ADCR_P_HPL), val << ADCR_P_HPL);
	}
	if (sprd_codec->sprd_linein_headphone_set & BIT(SPRD_CODEC_ADCRHEADPHONE_R_SET)) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC7), BIT(ADCR_P_HPR), val << ADCR_P_HPR);
	}
}

static int _mixer_adc_linein_set(struct snd_soc_codec *codec, int id, unsigned int mask,
			    int on, unsigned int shift)
{
	int ret = 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	mutex_lock(&sprd_codec->sprd_linein_mute_mutex);
	if (on) {
		sprd_codec->sprd_linein_set |= BIT(id);
	} else {
		sprd_codec->sprd_linein_set &= ~(BIT(id));
	}
	if (!sprd_codec->sprd_linein_mute) {
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), mask, on << shift);
	}
	mutex_unlock(&sprd_codec->sprd_linein_mute_mutex);
	return ret;
}

static int _mixer_linein_dac_set(struct snd_soc_codec *codec, int id, unsigned short reg, unsigned int mask,
			    int on, unsigned int shift)
{
	int ret = 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	mutex_lock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	if (on) {
		if (SOC_REG(ANA_CDC6) == reg)
			sprd_codec->sprd_linein_speaker_set |= BIT(id);
		else if (SOC_REG(ANA_CDC7) == reg)
			sprd_codec->sprd_linein_headphone_set |= BIT(id);
	} else {
		if (SOC_REG(ANA_CDC6) == reg)
			sprd_codec->sprd_linein_speaker_set &= ~(BIT(id));
		else if (SOC_REG(ANA_CDC7) == reg)
			sprd_codec->sprd_linein_headphone_set &= ~(BIT(id));
	}
	if (((!sprd_codec->sprd_linein_speaker_mute) && (SOC_REG(ANA_CDC6) == reg)) ||
		((!sprd_codec->sprd_linein_headphone_mute) && (SOC_REG(ANA_CDC7) == reg))) {
		ret = snd_soc_update_bits(codec, reg, mask, on << shift);
	}
	mutex_unlock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	return ret;
}

/* adc mixer */

static int ailadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_adc_linein_set(codec, SPRD_CODEC_AILADCL_SET, BIT(AIL_ADCL),
					on , AIL_ADCL);
}

static int ailadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_adc_linein_set(codec, SPRD_CODEC_AILADCR_SET, BIT(AIL_ADCR),
					on , AIL_ADCR);
}

static int airadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_adc_linein_set(codec, SPRD_CODEC_AIRADCL_SET, BIT(AIR_ADCL),
					on , AIR_ADCL);
}

static int airadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_adc_linein_set(codec, SPRD_CODEC_AIRADCR_SET, BIT(AIR_ADCR),
					on , AIR_ADCR);
}

static int mainmicadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), BIT(MIC_ADCL),
				   on << MIC_ADCL);
}

static int mainmicadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), BIT(MIC_ADCR),
				   on << MIC_ADCR);
}

static int auxmicadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				   BIT(AUXMIC_ADCL), on << AUXMIC_ADCL);
}

static int auxmicadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				   BIT(AUXMIC_ADCR), on << AUXMIC_ADCR);
}

static int hpmicadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				   BIT(HEADMIC_ADCL), on << HEADMIC_ADCL);
}

static int hpmicadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				   BIT(HEADMIC_ADCR), on << HEADMIC_ADCR);
}

/* hp mixer */

static int daclhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC7),
				   BIT(DACL_P_HPL), on << DACL_P_HPL);
}

static int daclhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC7),
				   BIT(DACL_N_HPR), on << DACL_N_HPR);
}

static int dacrhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC7),
				   BIT(DACR_P_HPL), on << DACR_P_HPL);
}

static int dacrhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC7),
				   BIT(DACR_P_HPR), on << DACR_P_HPR);
}

static int adclhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_linein_dac_set(codec, SPRD_CODEC_ADCLHEADPHONE_L_SET, SOC_REG(ANA_CDC7), BIT(ADCL_P_HPL),
					on , ADCL_P_HPL);
}

static int adclhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_linein_dac_set(codec, SPRD_CODEC_ADCLHEADPHONE_R_SET, SOC_REG(ANA_CDC7), BIT(ADCL_N_HPR),
					on , ADCL_N_HPR);
}

static int adcrhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_linein_dac_set(codec, SPRD_CODEC_ADCRHEADPHONE_L_SET, SOC_REG(ANA_CDC7), BIT(ADCR_P_HPL),
					on , ADCR_P_HPL);
}

static int adcrhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_linein_dac_set(codec, SPRD_CODEC_ADCRHEADPHONE_R_SET, SOC_REG(ANA_CDC7), BIT(ADCR_P_HPR),
					on , ADCR_P_HPR);
}

/* spkl mixer */

static int daclspkl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(DACL_AOL),
				   on << DACL_AOL);
}

static int dacrspkl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(DACR_AOL),
				   on << DACR_AOL);
}

static int adclspkl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_linein_dac_set(codec, SPRD_CODEC_ADCLSPEAKER_L_SET, SOC_REG(ANA_CDC6), BIT(ADCL_AOL),
					on , ADCL_AOL);
}

static int adcrspkl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return _mixer_linein_dac_set(codec, SPRD_CODEC_ADCRSPEAKER_L_SET, SOC_REG(ANA_CDC6), BIT(ADCR_AOL),
					on , ADCR_AOL);
}

/* spkr mixer */

static int daclspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(DACL_AOR),
				   on << DACL_AOR);
}

static int dacrspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(DACR_AOR),
				   on << DACR_AOR);
}

static int adclspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(ADCL_AOR),
				   on << ADCL_AOR);
}

static int adcrspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), BIT(ADCR_AOR),
				   on << ADCR_AOR);
}

/* ear mixer */

static int daclear_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC7), BIT(DACL_EAR),
				   on << DACL_EAR);
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
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU0), mask, val);
}

static inline void __sprd_codec_speaker_pa_ibias_set(struct snd_soc_codec *codec, int c_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s %d\n", __func__, c_sel);
	mask = PA_AB_I_MASK << PA_AB_I;
	val = (c_sel << PA_AB_I) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU3), mask, val);
}

static void sprd_inter_speaker_pa_pre(struct snd_soc_codec *codec)
{
	/* set class-AB Ibias to 7.4/4.7 from weifeng */
	__sprd_codec_speaker_pa_ibias_set(codec, 0);
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
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC4), mask, val);
}

static inline void sprd_codec_pa_demi_en(struct snd_soc_codec *codec, int on)
{
	int mask;

	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	if (on)
		/*when emi is on, need to set this register from xun && junjie.chen*/
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU6), 0xffff, 0x400);
	else
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU6), 0xffff, 0);

	mask = BIT(PA_DEMI_EN);
	/* Slew rate changing */
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), BIT(DACBUF_I_S), 0xec50);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), DACDC_RMV_S_MASK << DACDC_RMV_S, 0xec50);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC4), mask, 0xf000);


	/*open ocp from xun && weifeng*/
	mask = BIT(DRV_OCP_AOL_PD) | BIT(DRV_OCP_AOR_PD);
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU6), mask, 0);
}

static inline void sprd_codec_pa_ldo_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	mask = BIT(PA_LDO_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU0), mask, val);
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
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU3), mask, val);
}

static inline void sprd_codec_pa_ovp_v_sel(struct snd_soc_codec *codec,
					   int v_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, v_sel);
	mask = PA_OVP_V_MASK << PA_OVP_V;
	val = (v_sel << PA_OVP_V) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU5), mask, val);
}

static inline void sprd_codec_pa_dtri_f_sel(struct snd_soc_codec *codec,
					    int f_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, f_sel);
	mask = PA_DTRI_F_MASK << PA_DTRI_F;
	val = (f_sel << PA_DTRI_F) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC4), mask, val);
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
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU0), mask, val);
	spin_unlock(&sprd_codec->sprd_codec_pa_sw_lock);
}

static inline void sprd_codec_linein_mute_init(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec->sprd_linein_mute = 0;
	sprd_codec->sprd_linein_set = 0;
	mutex_init(&sprd_codec->sprd_linein_mute_mutex);
	sprd_codec->sprd_linein_speaker_mute = 0;
	sprd_codec->sprd_linein_speaker_set = 0;
	sprd_codec->sprd_linein_headphone_mute = 0;
	sprd_codec->sprd_linein_headphone_set = 0;
	mutex_init(&sprd_codec->sprd_linein_speaker_mute_mutex);
}

static inline void sprd_codec_inter_pa_init(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec->inter_pa.setting.LDO_V_sel = 0x03;
	sprd_codec->inter_pa.setting.DTRI_F_sel = 0x01;
}

static void sprd_codec_ovp_irq_enable(struct snd_soc_codec *codec)
{
	int val_clr, val_en;
	val_clr = BIT(OVP_IRQ) << AUD_A_INT_CLR;
	val_en = BIT(OVP_IRQ) << AUD_A_INT_EN;
	snd_soc_update_bits(codec, SOC_REG(DIG_CFG3), val_clr, val_clr);
	snd_soc_update_bits(codec, SOC_REG(DIG_CFG4), val_en, val_en);
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
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), BIT(AOL_EN), 0);
	} else {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), BIT(AOL_EN),
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

	if (!(snd_soc_read(codec, ANA_STS0) & BIT(OVP_FLAG))) {
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
		sprd_inter_speaker_pa_pre(codec);
		sprd_codec_pa_ovp_v_sel(codec, PA_OVP_496);
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
		/* delay 20ms as weifeng's suggestion */
		sprd_codec_wait(20);
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

#define SPRD_CODEC_HP_VER(bit) \
do { \
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec); \
	switch(sprd_codec->hp_ver) { \
	case SPRD_CODEC_HP_PA_VER_2: \
		mask = BIT(V2_##bit); \
		break; \
	case SPRD_CODEC_HP_PA_VER_1: \
	default: \
		mask = BIT(bit); \
		break; \
	} \
} while(0)

static inline void sprd_codec_hp_classg_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	//SPRD_CODEC_HP_VER(AUDIO_CLASSG_EN);
	mask = BIT(CG_REF_EN);
	val = on ? mask : 0;
    /* FIXME: change by jian.chen */
	//snd_soc_update_bits(codec, SOC_REG(DCR8_DCR7), mask, val);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_hp_pa_lpw(struct snd_soc_codec *codec, int lpw)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, lpw);
    /* FIXME: change by jian.chen */
    /*
	switch (sprd_codec->hp_ver) {
	case SPRD_CODEC_HP_PA_VER_2:
		if (sci_get_ana_chip_ver() >= SC2711BA) {
			mask = V2_AUDIO_CHP_LPW_MASK_BA  << V2_AUDIO_CHP_LPW_BA;
			val = (lpw << V2_AUDIO_CHP_LPW_BA) & mask;
		} else {
			mask = V2_AUDIO_CHP_LPW_MASK  << V2_AUDIO_CHP_LPW;
			val = (lpw << V2_AUDIO_CHP_LPW) & mask;
		}
		break;
	case SPRD_CODEC_HP_PA_VER_1:
	default:
		if (sci_get_ana_chip_ver() >= SC2713CA) {
			mask = AUDIO_CHP_LPW_MASK_CA << AUDIO_CHP_LPW_CA;
			val = (lpw << AUDIO_CHP_LPW_CA) & mask;
		} else {
			mask = AUDIO_CHP_LPW_MASK << AUDIO_CHP_LPW;
			val = (lpw << AUDIO_CHP_LPW) & mask;
		}
		break;
	}
	*/
	mask = CG_LPW_MASK << CG_LPW;
	val = (lpw << CG_LPW) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_hp_pa_mode(struct snd_soc_codec *codec, int on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
    /* FIXME: change by jian.chen */
    /*
	switch (sprd_codec->hp_ver) {
	case SPRD_CODEC_HP_PA_VER_2:
		if (sci_get_ana_chip_ver() >= SC2711BA)
			mask = BIT(V2_AUDIO_CHP_MODE_BA );
		else
			mask = BIT(V2_AUDIO_CHP_MODE);
		break;
	case SPRD_CODEC_HP_PA_VER_1:
	default:
		if (sci_get_ana_chip_ver() >= SC2713CA)
			mask = BIT(AUDIO_CHP_MODE_CA );
		else
			mask = BIT(AUDIO_CHP_MODE );
		break;
	}
    */
	mask = BIT(CG_HP_MODE);
	val = on? mask :0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_hp_pa_osc(struct snd_soc_codec *codec, int osc)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, osc);
    /* FIXME: change by jian.chen */
    /*
	switch (sprd_codec->hp_ver) {
	case SPRD_CODEC_HP_PA_VER_2:
		mask = V2_AUDIO_CHP_OSC_MASK << V2_AUDIO_CHP_OSC;
		val = (osc << V2_AUDIO_CHP_OSC) & mask;
		break;
	case SPRD_CODEC_HP_PA_VER_1:
	default:
		mask = AUDIO_CHP_OSC_MASK << AUDIO_CHP_OSC;
		val = (osc << AUDIO_CHP_OSC) & mask;
		break;
	}
    */
	mask = CG_CHP_OSC_MASK << CG_CHP_OSC;
	val = (osc << CG_CHP_OSC) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_hp_pa_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
    /* FIXME: change by jian.chen */
    /*
	SPRD_CODEC_HP_VER(AUDIO_CHP_EN);
	*/
	mask = BIT(CG_CHP_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_hp_pa_hpl_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
    /* FIXME: change by jian.chen */
    /*
	SPRD_CODEC_HP_VER(AUDIO_CHP_HPL_EN);
	*/
	mask = BIT(CG_HPL_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_hp_cg_ldo_en(struct snd_soc_codec *codec, int on)
{
	int ret = 0;
	struct regulator **regu;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	if(!sprd_codec->cg_ldo) {
		ret = sprd_codec_power_get(0, &sprd_codec->cg_ldo, "CGLDO");
		if (ret) {
			pr_err("ERR:%s get CGLDO error %d\n", __func__, ret);
			return;
		}
	}
	regu = &sprd_codec->cg_ldo;
	sprd_codec_regulator_set(regu, on);
}

static inline void sprd_codec_hp_cg_dangl_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	/* set this value from xun.zhang */
	mask = 0xFFFF;
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(DANGL), mask, val);
}

static inline void sprd_codec_hp_cg_dangr_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	/* set this value from xun.zhang */
	mask = 0xFFFF;
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(DANGR), mask, val);
}

static inline void sprd_codec_hp_pa_hpr_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
    /* FIXME: change by jian.chen */
    /*
	SPRD_CODEC_HP_VER(AUDIO_CHP_HPR_EN);
	*/
	mask = BIT(CG_HPR_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_hp_pa_hpl_mute(struct snd_soc_codec *codec,
					     int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
    /* FIXME: change by jian.chen */
    /*
	SPRD_CODEC_HP_VER(AUDIO_CHP_LMUTE);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(DCR8_DCR7), mask, val);
	*/
}

static inline void sprd_codec_hp_pa_hpr_mute(struct snd_soc_codec *codec,
					     int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
    /* FIXME: change by jian.chen */
    /*
	SPRD_CODEC_HP_VER(AUDIO_CHP_RMUTE);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(DCR8_DCR7), mask, val);
	*/
}

static inline void sprd_codec_hp_pa_cgcal_en(struct snd_soc_codec *codec,
					     int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
    /* FIXME: change by jian.chen */
    /*
	SPRD_CODEC_HP_VER(AUDIO_CGCAL_EN);
	*/
	mask = BIT(CG_HPCAL_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);
}

static inline void sprd_codec_ldocg_fal_ild_en(struct snd_soc_codec *codec,
						int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);

	mask = BIT(LDOCG_EN_FAL_ILD);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), mask, val);
}

static inline void sprd_codec_ldocg_iset_fal_current_sel(struct snd_soc_codec *codec,
						int i_sel)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, i_sel);

	mask = BIT(LDOCG_ISET_FAL);
	val = i_sel ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), mask, val);
}

static inline void sprd_codec_ldocg_ldocg_ramp_en(struct snd_soc_codec *codec,
						int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);

	mask = BIT(LDOCG_RAMP_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), mask, val);
}

static inline void sprd_codec_head_l_int_pu_pd(struct snd_soc_codec *codec,
						int on)
{
	int mask;
	int val;
	sp_asoc_pr_dbg("%s set %d\n", __func__, on);

	mask = BIT(AUD_HEAD_L_INT_PU_PD);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_HDT1), mask, val);
}

static inline void sprd_codec_inter_hp_pa_init(struct sprd_codec_priv
					       *sprd_codec)
{
	sprd_codec->inter_hp_pa.setting.class_g_osc = 0x01;
	sprd_codec->inter_hp_pa.setting.class_g_cgcal = 1;
	sprd_codec->inter_hp_pa.setting.class_g_pa_en_delay_10ms = 1;
	sprd_codec->inter_hp_pa.setting.class_g_hp_switch_delay_10ms = 3;
	sprd_codec->inter_hp_pa.setting.class_g_hp_on_delay_20ms = 2;

	//sprd_codec->inter_hp_pa.setting.class_g_open_delay_10ms = 0;
	//sprd_codec->inter_hp_pa.setting.class_g_vdd_delay_10ms = 0;
	//sprd_codec->inter_hp_pa.setting.class_g_chp_delay_30ms = 0;
	//sprd_codec->inter_hp_pa.setting.class_g_all_close_delay_100ms = 0;
	/* set class-G default pga from weifeng */
	sprd_codec->pga[SPRD_CODEC_PGA_CG_HPL_1].pgaval = 0x10;
	sprd_codec->pga[SPRD_CODEC_PGA_CG_HPR_1].pgaval = 0x10;
	sprd_codec->pga[SPRD_CODEC_PGA_CG_HPL_2].pgaval = 0x7;
	sprd_codec->pga[SPRD_CODEC_PGA_CG_HPR_2].pgaval = 0x7;
}


static void sprd_inter_headphone_pa_pre(struct sprd_codec_priv *sprd_codec,int on)
{
	struct snd_soc_codec *codec = sprd_codec->codec;
	static struct regulator *regulator = 0;
	struct sprd_codec_inter_hp_pa *p_setting =
		&sprd_codec->inter_hp_pa.setting;

	static struct regulator *regulator_reg= 0;

#ifdef CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
	static struct regulator *regulator_vb= 0;
	int classg_vol = 2200000;
#endif
      return ;
#if 0
	if (on) {
		if (!regulator) {
			sp_asoc_pr_dbg("%s set vddclsg on\n", __func__);
			/*1.CG_EN */
			//open clk
			regulator_reg = regulator_get(0, "VREG");
			if (IS_ERR(regulator_reg)) {
				pr_err("ERR:Failed to request %ld: %s\n",
					   PTR_ERR(regulator_reg), "VREG");
				BUG_ON(1);
			}
			regulator_set_mode(regulator_reg, REGULATOR_MODE_STANDBY);
			if (regulator_enable(regulator_reg) < 0) {
			} else {
#ifdef CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
				//open vb
				regulator_vb = regulator_get(0, "VB");
				if (IS_ERR(regulator_vb)) {
					pr_err("ERR:Failed to request %ld: %s\n",
						   PTR_ERR(regulator_vb), "VB");
					BUG_ON(1);
				}
				regulator_set_mode(regulator_vb, REGULATOR_MODE_STANDBY);
				if (regulator_enable(regulator_vb) < 0) {
				} else {
#endif
				sprd_codec_hp_classg_en(codec, 1);
#ifdef CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
				}
#endif
			}
			//close clk
			regulator_set_mode(regulator_reg,
					   REGULATOR_MODE_NORMAL);
			regulator_disable(regulator_reg);
			regulator_put(regulator_reg);
			regulator_reg = 0;

			//sprd_codec_wait(p_setting->class_g_open_delay_10ms * 10);
			/*2. LDO PD */
			regulator = regulator_get(0, CLASS_G_LDO_ID);
			if (IS_ERR(regulator)) {
				pr_err("ERR:Failed to request %ld: %s\n",
					   PTR_ERR(regulator), CLASS_G_LDO_ID);
				BUG_ON(1);
			}
			regulator_set_mode(regulator, REGULATOR_MODE_STANDBY);
#ifdef CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
			if (sprd_codec->hp_ver == SPRD_CODEC_HP_PA_VER_1)
				classg_vol = 1200000;
			regulator_set_voltage(regulator, classg_vol, classg_vol);
#endif
			if (regulator_enable(regulator) < 0) {
				regulator_set_mode(regulator,
						   REGULATOR_MODE_NORMAL);
				regulator_disable(regulator);
				regulator_put(regulator);
				regulator = 0;
			}
		}
	} else{
		if (regulator) {
			sp_asoc_pr_dbg("%s set vddclsg off\n", __func__);
			/*LDO PD*/
			regulator_set_mode(regulator, REGULATOR_MODE_NORMAL);
			regulator_disable(regulator);
			regulator_put(regulator);
			regulator = 0;
			/*CG_EN*/
			//open clk
			regulator_reg = regulator_get(0, "VREG");
			if (IS_ERR(regulator_reg)) {
				pr_err("ERR:Failed to request %ld: %s\n",
					   PTR_ERR(regulator_reg), "VREG");
				BUG_ON(1);
			}
			regulator_set_mode(regulator_reg, REGULATOR_MODE_STANDBY);
			if (regulator_enable(regulator_reg) < 0) {
			} else {
				sprd_codec_hp_classg_en(codec, 0);
#ifdef CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
				if(regulator_vb) {
					//close vb
					regulator_set_mode(regulator_vb, REGULATOR_MODE_NORMAL);
					regulator_disable(regulator_vb);
					regulator_put(regulator_vb);
					regulator_vb = 0;
				}
#endif
			}
			//close clk
			regulator_set_mode(regulator_reg,
					   REGULATOR_MODE_NORMAL);
			regulator_disable(regulator_reg);
			regulator_put(regulator_reg);
			regulator_reg = 0;
		}
	}
#endif
}
static int  hp_notifier_handler (struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct sprd_codec_priv *sprd_codec = container_of (nb, struct sprd_codec_priv, nb);
	sprd_inter_headphone_pa_pre (sprd_codec, !!action);
	return 0;
}

static int sprd_inter_headphone_pa(struct snd_soc_codec *codec, int on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct sprd_codec_inter_hp_pa *p_setting =
	    &sprd_codec->inter_hp_pa.setting;
	/*static struct regulator *regulator = 0;*/
	sp_asoc_pr_info("inter HP PA Switch %s\n", STR_ON_OFF(on));
	mutex_lock(&sprd_codec->inter_hp_pa_mutex);
	if (on) {
		sprd_codec_hp_pa_lpw(codec, p_setting->class_g_low_power);

		sprd_codec_hp_pa_osc(codec, p_setting->class_g_osc);
		/* CG_REF_EN */
		sprd_codec_hp_classg_en(codec, 1);
		/* LDOCG_EN */
		sprd_codec_hp_cg_ldo_en(codec, 1);
		/* CHP EN */
		sprd_codec_hp_pa_en(codec, 1);

		sprd_codec_ldocg_fal_ild_en(codec, 1);
		sprd_codec_ldocg_iset_fal_current_sel(codec, 1);
		sprd_codec_ldocg_ldocg_ramp_en(codec, 1);

		/* Head_L_INT pull up power down */
		sprd_codec_head_l_int_pu_pd(codec, 1);
		/* disable DANGL/R in 2723AB */
		if (AUDIO_2723_VER_AA == sci_get_ana_chip_ver()) {
			sprd_codec_hp_cg_dangl_en(codec, 1);
			sprd_codec_hp_cg_dangr_en(codec, 1);
		}
		/* Set CG as weifeng's suggestion */
		sprd_codec_pga_cg_hpl_1_set(codec, 0x10);
		sprd_codec_pga_cg_hpr_1_set(codec, 0x10);
		sprd_codec_pga_cg_hpl_2_set(codec, 0x7);
		sprd_codec_pga_cg_hpr_2_set(codec, 0x7);

		sprd_codec_wait(p_setting->class_g_hp_switch_delay_10ms * 10);

	} else {
		/* CG_REF disable */
		sprd_codec_hp_classg_en(codec, 0);

		/* LDOCG disable */
		sprd_codec_hp_cg_ldo_en(codec, 0);
		/* CHP disable */
		sprd_codec_hp_pa_en(codec, 0);
		sprd_codec_ldocg_fal_ild_en(codec, 0);
		sprd_codec_ldocg_iset_fal_current_sel(codec, 0);
		sprd_codec_ldocg_ldocg_ramp_en(codec, 0);

		/* DANGL/R disable */
		if (AUDIO_2723_VER_AA == sci_get_ana_chip_ver()) {
			sprd_codec_hp_cg_dangl_en(codec, 0);
			sprd_codec_hp_cg_dangr_en(codec, 0);
		}
	}
	mutex_unlock(&sprd_codec->inter_hp_pa_mutex);
	return 0;
}

static int sprd_inter_headphone_pa_post(struct snd_soc_codec *codec, int on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct sprd_codec_inter_hp_pa *p_setting =
	    &sprd_codec->inter_hp_pa.setting;
	mutex_lock(&sprd_codec->inter_hp_pa_mutex);
	if (on) {
		/*open classG mute delay time */
		//sprd_codec_wait(p_setting->class_g_unmute_delay_100ms * 100);
		/* L/R EN */
		sprd_codec_hp_pa_mode(codec, p_setting->class_g_mode);
		sprd_codec_hp_pa_cgcal_en(codec, p_setting->class_g_cgcal);

		sprd_codec_wait(p_setting->class_g_pa_en_delay_10ms * 10);
		sprd_codec_hp_pa_hpl_en(codec, 1);
		sprd_codec_hp_pa_hpr_en(codec, 1);
		sprd_codec_wait(p_setting->class_g_hp_on_delay_20ms * 20);

		/*unmute */
		sprd_codec_hp_pa_hpl_mute(codec, 0);
		sprd_codec_hp_pa_hpr_mute(codec, 0);

		sprd_codec->inter_hp_pa.set = 1;
	} else {
		sprd_codec->inter_hp_pa.set = 0;
		/*mute */
		sprd_codec_hp_pa_hpl_mute(codec, 1);
		sprd_codec_hp_pa_hpr_mute(codec, 1);
		sprd_codec_hp_pa_hpr_en(codec, 0); 
		sprd_codec_hp_pa_hpl_en(codec, 0);
		sprd_codec_wait(10);
		sprd_codec_hp_pa_cgcal_en(codec, 0);
		sprd_codec_hp_pa_mode(codec, 0);
	}
	mutex_unlock(&sprd_codec->inter_hp_pa_mutex);
	return 0;
}

#ifdef CONFIG_SND_SOC_SPRD_AUDIO_USE_INTER_HP_PA_V4
static int hp_pa_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	return sprd_inter_headphone_pa(w->codec, on);
}

static int hp_pa_post_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	return sprd_inter_headphone_pa_post(w->codec, on);
}
#else
static int hp_pa_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}

static int hp_pa_post_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}
#endif

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
#ifdef CONFIG_SND_SOC_VBC_SRC_SAMPLE_RATE
	if (rate == 44100)
		rate = CONFIG_SND_SOC_VBC_SRC_SAMPLE_RATE;
#endif
	set = rate / 4000;
	if (set > 13) {
		pr_err("ERR:SPRD-CODEC not support this ad rate %d\n", rate);
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC_CTL), mask, set << shift);
	return 0;
}

static int sprd_codec_sample_rate_setting(struct sprd_codec_priv *sprd_codec)
{
	sp_asoc_pr_dbg("%s AD %d DA %d AD1 %d\n", __func__,
		       sprd_codec->ad_sample_val, sprd_codec->da_sample_val,
		       sprd_codec->ad1_sample_val);
	if (sprd_codec->ad_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
					      sprd_codec->ad_sample_val, 0x0F,
					      0);
	}
	if (sprd_codec->ad1_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
					      sprd_codec->ad1_sample_val, 0xF0,
					      4);
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
	struct regulator **regu = &sprd_codec->head_mic;
	sp_asoc_pr_dbg("%s\n", __func__);

	mutex_lock(&ldo_mutex);
	atomic_inc(&sprd_codec->ldo_refcount);
	if (atomic_read(&sprd_codec->ldo_refcount) == 1) {
		sp_asoc_pr_dbg("LDO ON!\n");
		if (*regu) {
			regulator_set_mode(*regu, REGULATOR_MODE_NORMAL);
		}
		arch_audio_codec_analog_enable();
		sprd_codec_vcom_ldo_auto(sprd_codec->codec, 1);
	}
	mutex_unlock(&ldo_mutex);

	return 0;
}

static int sprd_codec_ldo_off(struct sprd_codec_priv *sprd_codec)
{
	struct regulator **regu = &sprd_codec->head_mic;
	sp_asoc_pr_dbg("%s\n", __func__);

	mutex_lock(&ldo_mutex);
	if (atomic_dec_and_test(&sprd_codec->ldo_refcount)) {
		if (*regu && regulator_is_enabled(*regu)) {
			regulator_set_mode(*regu, REGULATOR_MODE_STANDBY);
		}
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

	/* SC7710/SC8830 ask from ASIC to set initial value */
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), BIT(SEL_VCMI),
			    BIT(SEL_VCMI));
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), BIT(VCMI_FAST_EN),
			    BIT(VCMI_FAST_EN));

    snd_soc_update_bits(codec, SOC_REG(ANA_PMU0), BIT(OVP_LDO),
			    BIT(OVP_LDO));
	return ret;
}

static int sprd_codec_digital_open(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sp_asoc_pr_dbg("%s\n", __func__);

	/* FIXME : Some Clock SYNC bug will cause MUTE */
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), BIT(DAC_MUTE_EN), 0);
    sprd_codec_dodvl_set(codec,0x7);
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

    snd_soc_update_bits(codec, SOC_REG(ANA_PMU0), BIT(OVP_LDO),
			    0);
	sprd_codec_ldo_off(sprd_codec);
}

static void sprd_codec_adc_clock_input_en(struct snd_soc_codec *codec, int on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s, on:%d \n", __func__, on);

	if (on) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), BIT(ADC_CLK_EN), BIT(ADC_CLK_EN));
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), BIT(ADC_CLK_RST), BIT(ADC_CLK_RST));
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), BIT(ADC_CLK_RST), 0);
	} else {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), BIT(ADC_CLK_EN), 0);
	}
}


static int digital_power_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		arch_audio_codec_digital_reg_enable();
		arch_audio_codec_digital_enable();
		arch_audio_codec_digital_reset();
		sprd_codec_digital_open(w->codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*maybe ADC module use it, so we cann't close it */
		/*arch_audio_codec_digital_disable(); */
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

static int audio_adc_clock_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_adc_clock_input_en(codec, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_adc_clock_input_en(codec, 0);
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

static int dfm_out_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_info("DFM-OUT %s\n", STR_ON_OFF(on));
	sprd_codec->ad_sample_val = sprd_codec->da_sample_val;
	sprd_codec_sample_rate_setting(sprd_codec);

	return 0;
}

static int _mixer_set_mixer(struct snd_soc_codec *codec, int id, int lr,
			    int try_on, int need_set)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int reg = GET_MIXER_ID(ID_FUN(id, lr));
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
	id = (id >>1) + SPRD_CODEC_MIXER_START;
	return _mixer_setting(codec, id, id + 1, lr, try_on, need_set);
}

#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
static void sprd_codec_hp_pop_irq_enable(struct snd_soc_codec *codec)
{
	int val_clr, val_en;
	val_clr = BIT(AUDIO_POP_IRQ) << AUD_A_INT_CLR;
	val_en = BIT(AUDIO_POP_IRQ) << AUD_A_INT_EN;
	snd_soc_update_bits(codec, SOC_REG(DIG_CFG3), val_clr, val_clr);
	snd_soc_update_bits(codec, SOC_REG(DIG_CFG4), val_en, val_en);
}
#endif

static irqreturn_t sprd_codec_ap_irq(int irq, void *dev_id)
{
	int mask;
	struct sprd_codec_priv *sprd_codec = dev_id;
	struct snd_soc_codec *codec = sprd_codec->codec;
	mask = (snd_soc_read(codec, DIG_STS1) >> AUD_IRQ_MSK);
	sp_asoc_pr_dbg("HP POP IRQ Mask = 0x%x\n", mask);
	if (BIT(AUDIO_POP_IRQ) & mask) {
		snd_soc_update_bits(codec, SOC_REG(DIG_CFG4), BIT(AUDIO_POP_IRQ) << AUD_A_INT_EN, 0);
		complete(&sprd_codec->completion_hp_pop);
	}
	if (BIT(OVP_IRQ) & mask) {
		snd_soc_update_bits(codec, SOC_REG(DIG_CFG4), BIT(OVP_IRQ) << AUD_A_INT_EN, 0);
		sprd_codec_ovp_start(sprd_codec, 1);
		sprd_codec_ovp_irq_enable(codec);
	}
	return IRQ_HANDLED;
}

#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
static inline int is_hp_pop_compelet(struct snd_soc_codec *codec)
{
	int val;
	val = snd_soc_read(codec, ANA_STS0);
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
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_STEP_2 << HP_POP_STEP);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_CTL_UP << HP_POP_CTL);
		sp_asoc_pr_dbg("U PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, ANA_CDC16));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mask = HP_POP_STEP_MASK << HP_POP_STEP;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_STEP_1 << HP_POP_STEP);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_CTL_DOWN << HP_POP_CTL);
		sp_asoc_pr_dbg("D PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, ANA_CDC16));
		break;
	case SND_SOC_DAPM_POST_PMU:
		ret = hp_pop_wait_for_compelet(codec);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_CTL_HOLD << HP_POP_CTL);
		sp_asoc_pr_dbg("HOLD PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, ANA_CDC16));
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = hp_pop_wait_for_compelet(codec);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_CTL_DIS << HP_POP_CTL);
		sp_asoc_pr_dbg("DIS PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, ANA_CDC16));
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
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct sprd_codec_inter_hp_pa *p_setting =
	    &sprd_codec->inter_hp_pa.setting;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
#if 1				/* enable the diff function in shark from weifeng.ni */
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), BIT(DIFF_EN),
				    BIT(DIFF_EN));
        /* FIXME: change by jian.chen */
/*

		snd_soc_update_bits(codec, SOC_REG(DCR2_DCR1), BIT(HP_VCMI_EN),
			BIT(HP_VCMI_EN));
*/
#endif
        //sprd_codec_wait(p_setting->class_g_hp_on_delay_20ms * 20);
#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_CTL_DIS << HP_POP_CTL);
		sp_asoc_pr_dbg("DIS(en) PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, ANA_CDC16));
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), BIT(DIFF_EN), 0);
        /* FIXME: change by jian.chen */
        /*
		snd_soc_update_bits(codec, SOC_REG(DCR2_DCR1), BIT(HP_VCMI_EN), 0);
		*/

#ifndef CONFIG_SND_SOC_SPRD_CODEC_NO_HP_POP
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask,
				    HP_POP_CTL_HOLD << HP_POP_CTL);
		sp_asoc_pr_dbg("HOLD(en) PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, ANA_CDC16));
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
		       snd_soc_read(codec, ANA_CDC2) & BIT(HPL_EN), 1);

	_mixer_setting(codec, SPRD_CODEC_HP_DACL,
		       SPRD_CODEC_HP_DAC_MIXER_MAX, SPRD_CODEC_RIGHT,
		       snd_soc_read(codec, ANA_CDC2) & BIT(HPR_EN), 1);

	_mixer_setting(codec, SPRD_CODEC_HP_ADCL,
		       SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_LEFT,
		       snd_soc_read(codec, ANA_CDC2) & BIT(HPL_EN), 0);

	_mixer_setting(codec, SPRD_CODEC_HP_ADCL,
		       SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_RIGHT,
		       snd_soc_read(codec, ANA_CDC2) & BIT(HPR_EN), 0);

_pre_pmd:

	return ret;
}

static int spk_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (snd_soc_read(codec, ANA_CDC2) & BIT(AOL_EN)) {
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
		       (snd_soc_read(codec, ANA_CDC2) & BIT(AOL_EN)), 1);

	_mixer_setting(codec, SPRD_CODEC_SPK_DACL,
		       SPRD_CODEC_SPK_DAC_MIXER_MAX, SPRD_CODEC_RIGHT,
		       (snd_soc_read(codec, ANA_CDC2) & BIT(AOR_EN)), 1);

	_mixer_setting(codec, SPRD_CODEC_SPK_ADCL,
		       SPRD_CODEC_SPK_MIXER_MAX, SPRD_CODEC_LEFT,
		       (snd_soc_read(codec, ANA_CDC2) & BIT(AOL_EN)), 0);

	_mixer_setting(codec, SPRD_CODEC_SPK_ADCL,
		       SPRD_CODEC_SPK_MIXER_MAX, SPRD_CODEC_RIGHT,
		       (snd_soc_read(codec, ANA_CDC2) & BIT(AOR_EN)), 0);

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
		sprd_codec_pa_sw_set(w->codec, SPRD_CODEC_PA_SW_EAR);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_pa_sw_clr(w->codec, SPRD_CODEC_PA_SW_EAR);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}
#endif

static int adcpgal_set(struct snd_soc_codec *codec, int on)
{
	int mask = ADCPGAL_EN_MASK << ADCPGAL_EN;
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC1), mask,
				   on ? mask : 0);
}

static int adcpgar_set(struct snd_soc_codec *codec, int on)
{
	int mask = ADCPGAR_EN_MASK << ADCPGAR_EN;
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC1), mask,
				   on ? mask : 0);
}

static int adc_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int is_right = (w->shift == SPRD_CODEC_RIGHT);
	int on = (! !SND_SOC_DAPM_EVENT_ON(event));
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (is_right) {
		adcpgar_set(codec, on);
		_mixer_setting(codec, SPRD_CODEC_AIL, SPRD_CODEC_ADC_MIXER_MAX,
			       SPRD_CODEC_RIGHT, on, 1);
	} else {
		adcpgal_set(codec, on);
		_mixer_setting(codec, SPRD_CODEC_AIL, SPRD_CODEC_ADC_MIXER_MAX,
			       SPRD_CODEC_LEFT, on, 1);
	}

	return 0;
}

static int pga_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = GET_PGA_ID(FUN_REG(w->reg));
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
		if(min >= 0)
			ret = sprd_codec_pga_cfg[id].set(codec, min);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}
	return ret;
}

static int adcpgar_byp_set(struct snd_soc_codec *codec, int value)
{
	int mask = ADCPGAR_BYP_MASK << ADCPGAR_BYP;
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC1), mask,
				    value << ADCPGAR_BYP);
}

static int adcpgar_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	sp_asoc_pr_dbg("%s  Event is %s\n", __func__,  get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		adcpgar_byp_set(codec, 2);
		sprd_codec_wait(100);
		adcpgar_byp_set(codec, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
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
	int id = GET_MIXER_ID(FUN_REG(w->reg));
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
	case SND_SOC_DAPM_POST_PMD:
		s_need_wait = 1;
		mixer = &(sprd_codec->mixer[id]);
		if (mixer->set) {
			mixer->set(codec, mixer->on);
		}
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
	int reg = FUN_REG(w->reg);
	int ret = 0;
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	struct regulator **regu;

	sp_asoc_pr_dbg("%s %s Event is %s\n", __func__,
		       mic_bias_name[GET_MIC_BIAS_ID(reg)],
		       get_event_name(event));

	switch (reg) {
	case SPRD_CODEC_MIC_BIAS:
		regu = &sprd_codec->main_mic;
		break;
	case SPRD_CODEC_AUXMIC_BIAS:
		regu = &sprd_codec->aux_mic;
		break;
	case SPRD_CODEC_HEADMIC_BIAS:
		regu = &sprd_codec->head_mic;
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	ret = sprd_codec_regulator_set(regu, on);

	return ret;
}

static int mixer_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = GET_MIXER_ID(FUN_REG(w->reg));
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
	int id = GET_MIXER_ID(FUN_REG(mc->reg));
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
	int id = GET_MIXER_ID(FUN_REG(mc->reg));
	struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
	int ret = 0;

	if (mixer->on == ucontrol->value.integer.value[0])
		return 0;

	sp_asoc_pr_info("Set %s Switch %s\n", sprd_codec_mixer_debug_str[id],
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	/*notice the sequence */
	snd_soc_dapm_put_volsw(kcontrol, ucontrol);

	/*update reg: must be set after snd_soc_dapm_put_enum_double->change = snd_soc_test_bits(widget->codec, e->reg, mask, val); */

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
	SND_SOC_DAPM_PGA_S(xname, SPRD_CODEC_ANA_MIXER_ORDER, FUN_REG(xreg), 0, 0, ana_loop_event,\
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)

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
	SND_SOC_DAPM_SUPPLY_S("DA Clk", 3, SOC_REG(ANA_CDC0), DAC_CLK_EN, 0, NULL,
			      0),
	SND_SOC_DAPM_SUPPLY_S("DRV Clk", 4, SOC_REG(ANA_CDC0), DRV_CLK_EN, 0, NULL,
			      0),
	SND_SOC_DAPM_SUPPLY_S("AD IBUF", 3, SOC_REG(ANA_CDC1), ADC_IBUF_PD,
			      1,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AD Clk", 3, SND_SOC_NOPM, 0, 0,
			      audio_adc_clock_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("Digital DACL Switch", 4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_L, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital DACR Switch", 4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_R, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACL Switch", 5, SOC_REG(DIG_CFG0),
			   AUDIFA_DACL_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACR Switch", 5, SOC_REG(DIG_CFG0),
			   AUDIFA_DACR_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DACL Switch", 6, SOC_REG(ANA_CDC2), DACL_EN, 0,
			   NULL,
			   0),
	SND_SOC_DAPM_PGA_S("DACR Switch", 6, SOC_REG(ANA_CDC2), DACR_EN, 0,
			   NULL,
			   0),
	SND_SOC_DAPM_PGA_S("DACL Mute", 6, FUN_REG(SPRD_CODEC_PGA_DACL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("DACR Mute", 6, FUN_REG(SPRD_CODEC_PGA_DACR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
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
	SND_SOC_DAPM_PGA_S("HP POP", SPRD_CODEC_HP_POP_ORDER, SND_SOC_NOPM, 0,
			   0, hp_pop_event,
			   SND_SOC_DAPM_POST_PMU),
#else
	SND_SOC_DAPM_SUPPLY_S("HP POP", 5, SND_SOC_NOPM, 0, 0, hp_pop_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
#endif
	SND_SOC_DAPM_PGA_S("HPL Switch", SPRD_CODEC_HP_SWITCH_ORDER,
			   SOC_REG(ANA_CDC2), HPL_EN, 0,
			   hp_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR Switch", SPRD_CODEC_HP_SWITCH_ORDER,
			   SOC_REG(ANA_CDC2), HPR_EN, 0,
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
	SND_SOC_DAPM_PGA_S("SPKL Switch", 5, SOC_REG(ANA_CDC2), AOL_EN, 0,
			   spk_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("SPKR Switch", 5, SOC_REG(ANA_CDC2), AOR_EN, 0,
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
	SND_SOC_DAPM_PGA_S("EAR Switch", 6, SOC_REG(ANA_CDC2), EAR_EN, 0,
			   ear_switch_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
#else
	SND_SOC_DAPM_PGA_S("EAR Switch", 6, SOC_REG(ANA_CDC2), EAR_EN, 0, NULL,
			   0),
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
	SND_SOC_DAPM_PGA_S("ADie Digital ADCL Switch", 2, SOC_REG(DIG_CFG0),
			   AUDIFA_ADCL_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCR Switch", 2, SOC_REG(DIG_CFG0),
			   AUDIFA_ADCR_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADCL Switch", 1, SOC_REG(ANA_CDC1), ADCL_PD, 1,
			   NULL,
			   0),
	SND_SOC_DAPM_PGA_S("ADCR Switch", 1, SOC_REG(ANA_CDC1), ADCR_PD, 1,
			   adcpgar_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("ADCL PGA", SND_SOC_NOPM, SPRD_CODEC_LEFT, 0, NULL,
			   0,
			   adc_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ADCR PGA", SND_SOC_NOPM, SPRD_CODEC_RIGHT, 0, NULL,
			   0,
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

	SND_SOC_DAPM_PGA_S("MIC Boost", 3, FUN_REG(SPRD_CODEC_PGA_MIC), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("AUXMIC Boost", 3, FUN_REG(SPRD_CODEC_PGA_AUXMIC), 0,
			   0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HEADMIC Boost", 3, FUN_REG(SPRD_CODEC_PGA_HEADMIC),
			   0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("AIL Boost", 3, FUN_REG(SPRD_CODEC_PGA_AIL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("AIR Boost", 3, FUN_REG(SPRD_CODEC_PGA_AIR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MICBIAS_E("Mic Bias", FUN_REG(SPRD_CODEC_MIC_BIAS), 0, 0,
			       mic_bias_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MICBIAS_E("AuxMic Bias", FUN_REG(SPRD_CODEC_AUXMIC_BIAS),
			       0, 0,
			       mic_bias_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MICBIAS_E("HeadMic Bias", FUN_REG(SPRD_CODEC_HEADMIC_BIAS),
			       0, 0,
			       mic_bias_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("Digital ADC1L Switch", 5, SOC_REG(AUD_TOP_CTL),
			   ADC1_EN_L, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital ADC1R Switch", 5, SOC_REG(AUD_TOP_CTL),
			   ADC1_EN_R, 0, NULL, 0),
	SND_SOC_DAPM_ADC_E("ADC1", "Ext-Capture-Vaudio",
			   FUN_REG(SPRD_CODEC_CAPTRUE1),
			   0, 0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	/*add DMIC */
	SND_SOC_DAPM_PGA_S("Digital ADC DMIC In", 4, SOC_REG(AUD_TOP_CTL),
			   ADC_DMIC_SEL, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital ADC1 DMIC In", 4, SOC_REG(AUD_TOP_CTL),
			   ADC1_DMIC1_SEL, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DMIC Switch", 3, SOC_REG(AUD_DMIC_CTL),
			   ADC_DMIC_EN, 0,
			   NULL, 0),
	SND_SOC_DAPM_PGA_S("DMIC1 Switch", 3, SOC_REG(AUD_DMIC_CTL),
			   ADC1_DMIC1_EN, 0,
			   NULL, 0),

	SND_SOC_DAPM_PGA_S("HP PA Switch", SPRD_CODEC_HP_PA_ORDER, SND_SOC_NOPM,
			   0, 0, hp_pa_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HP PA POST Switch", SPRD_CODEC_HP_PA_POST_ORDER,
			   SND_SOC_NOPM, 0, 0, hp_pa_post_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Spk PA Switch", SPRD_CODEC_PA_ORDER, SND_SOC_NOPM,
			   0, 0,
			   spk_pa_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HPL CGL Switch", SPRD_CODEC_HP_PA_POST_ORDER, SOC_REG(ANA_CDC7),
			   HPL_CGL, 0, NULL,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HPR CGR Switch", SPRD_CODEC_HP_PA_POST_ORDER, SOC_REG(ANA_CDC7),
			   HPR_CGR, 0, NULL,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HPL CG Mute1", SPRD_CODEC_CG_PGA_ORDER, FUN_REG(SPRD_CODEC_PGA_CG_HPL_1),
	           0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR CG Mute1", SPRD_CODEC_CG_PGA_ORDER, FUN_REG(SPRD_CODEC_PGA_CG_HPR_1),
	           0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPL CG Mute2", SPRD_CODEC_CG_PGA_ORDER, FUN_REG(SPRD_CODEC_PGA_CG_HPL_2),
	           0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR CG Mute2", SPRD_CODEC_CG_PGA_ORDER, FUN_REG(SPRD_CODEC_PGA_CG_HPR_2),
	           0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_INPUT("DMIC"),
	SND_SOC_DAPM_INPUT("DMIC1"),
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

	{"ADC1", NULL, "AD Clk"},

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
	{"DACL Mute", NULL, "ADie Digital DACL Switch"},
	{"DACR Mute", NULL, "ADie Digital DACR Switch"},
	{"DACL Switch", NULL, "DACL Mute"},
	{"DACR Switch", NULL, "DACR Mute"},

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

	{"HP PA", NULL, "HP PA POST Switch"},
	{"HP PA POST Switch", NULL, "HP PA Switch"},
	{"HPL CGL Switch", NULL, "HPL Mute"},
	{"HPR CGR Switch", NULL, "HPR Mute"},
	{"HPL CG Mute1", NULL, "HPL CGL Switch"},
	{"HPR CG Mute1", NULL, "HPR CGR Switch"},
	{"HPL CG Mute2", NULL, "HPL CG Mute1"},
	{"HPR CG Mute2", NULL, "HPR CG Mute1"},
	{"HP PA Switch", NULL, "HPL CG Mute2"},
	{"HP PA Switch", NULL, "HPR CG Mute2"},

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
	{"ADCL Mixer", "AILADCL Switch", "AIL Boost"},
	{"ADCR Mixer", "AILADCR Switch", "AIL Boost"},
	{"ADCL Mixer", "AIRADCL Switch", "AIR Boost"},
	{"ADCR Mixer", "AIRADCR Switch", "AIR Boost"},
	{"AIL Boost", NULL, "AIL"},
	{"AIR Boost", NULL, "AIR"},
	/* Input */
	{"ADCL Mixer", "MainMICADCL Switch", "MIC Boost"},
	{"ADCR Mixer", "MainMICADCR Switch", "MIC Boost"},
	{"MIC Boost", NULL, "Mic Bias"},
	{"ADCL Mixer", "AuxMICADCL Switch", "AUXMIC Boost"},
	{"ADCR Mixer", "AuxMICADCR Switch", "AUXMIC Boost"},
	{"AUXMIC Boost", NULL, "AuxMic Bias"},
	{"ADCL Mixer", "HPMICADCL Switch", "HEADMIC Boost"},
	{"ADCR Mixer", "HPMICADCR Switch", "HEADMIC Boost"},
	{"HEADMIC Boost", NULL, "HeadMic Bias"},
	/* Captrue */
	{"ADCL Switch", NULL, "ADCL PGA"},
	{"ADCR Switch", NULL, "ADCR PGA"},
	{"ADie Digital ADCL Switch", NULL, "ADCL Switch"},
	{"ADie Digital ADCR Switch", NULL, "ADCR Switch"},
	{"Digital ADCL Switch", NULL, "ADie Digital ADCL Switch"},
	{"Digital ADCR Switch", NULL, "ADie Digital ADCR Switch"},
	{"ADC", NULL, "Digital ADCL Switch"},
	{"ADC", NULL, "Digital ADCR Switch"},

	{"Digital ADC1L Switch", NULL, "ADie Digital ADCL Switch"},
	{"Digital ADC1R Switch", NULL, "ADie Digital ADCR Switch"},
	{"ADC1", NULL, "Digital ADC1L Switch"},
	{"ADC1", NULL, "Digital ADC1R Switch"},

	{"Mic Bias", NULL, "MIC"},
	{"AuxMic Bias", NULL, "AUXMIC"},
	{"HeadMic Bias", NULL, "HPMIC"},

	/* DMIC0 */
	{"DMIC Switch", NULL, "DMIC"},
	{"Digital ADC DMIC In", NULL, "DMIC Switch"},
	{"Digital ADCL Switch", NULL, "Digital ADC DMIC In"},
	{"Digital ADCR Switch", NULL, "Digital ADC DMIC In"},
	/* DMIC1 */
	{"DMIC1 Switch", NULL, "DMIC1"},
	{"Digital ADC1 DMIC In", NULL, "DMIC1 Switch"},
	{"Digital ADC1L Switch", NULL, "Digital ADC1 DMIC In"},
	{"Digital ADC1R Switch", NULL, "Digital ADC1 DMIC In"},

	/* Bias independent */
	{"Mic Bias", NULL, "Analog Power"},
	{"AuxMic Bias", NULL, "Analog Power"},
	{"HeadMic Bias", NULL, "Analog Power"},
};

static int sprd_codec_vol_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int id = GET_PGA_ID(FUN_REG(mc->reg));
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[id]);
	int ret = 0;

	sp_asoc_pr_info("Set PGA[%s] to %ld\n", sprd_codec_pga_debug_str[id],
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
	unsigned int id = GET_PGA_ID(FUN_REG(mc->reg));
	int max = mc->max;
	unsigned int invert = mc->invert;
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[id]);

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

static int sprd_codec_info_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	int ret = 0;

	sp_asoc_pr_info("%s, %d\n",
		__func__,(int)ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	if (SPRD_CODEC_INFO != val) {
		sp_asoc_pr_info("%s, wrong codec info (%d) \n", __func__, val);
	}
	return ret;
}

static int sprd_codec_info_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max;
	unsigned int invert = mc->invert;

	ucontrol->value.integer.value[0] = SPRD_CODEC_INFO;
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}
	return 0;
}

static int sprd_codec_linein_mute_put(struct snd_kcontrol *kcontrol,
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
	int i = 0;

	sp_asoc_pr_info("%s, 0x%08x\n",
		__func__,(int)ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mutex_lock(&sprd_codec->sprd_linein_mute_mutex);
	sprd_codec->sprd_linein_mute = (u32) val;
	if (sprd_codec->sprd_linein_set) {
		_mixer_adc_linein_mute_nolock(codec, sprd_codec->sprd_linein_mute);
	}
	mutex_unlock(&sprd_codec->sprd_linein_mute_mutex);
	return ret;
}

static int sprd_codec_linein_mute_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int invert = mc->invert;

	mutex_lock(&sprd_codec->sprd_linein_mute_mutex);
	ucontrol->value.integer.value[0] = sprd_codec->sprd_linein_mute;
	mutex_unlock(&sprd_codec->sprd_linein_mute_mutex);
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}
	return 0;
}

static int sprd_codec_linein_speaker_mute_put(struct snd_kcontrol *kcontrol,
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
	int i = 0;

	sp_asoc_pr_info("%s, 0x%08x\n",
		__func__,(int)ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mutex_lock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	sprd_codec->sprd_linein_speaker_mute = (u32) val;
	if (sprd_codec->sprd_linein_speaker_set) {
		_mixer_linein_speaker_mute_nolock(codec, sprd_codec->sprd_linein_speaker_mute);
	}
	mutex_unlock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	return ret;
}

static int sprd_codec_linein_speaker_mute_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int invert = mc->invert;

	mutex_lock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	ucontrol->value.integer.value[0] = sprd_codec->sprd_linein_speaker_mute;
	mutex_unlock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}
	return 0;
}

static int sprd_codec_linein_headphone_mute_put(struct snd_kcontrol *kcontrol,
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
	int i = 0;

	sp_asoc_pr_info("%s, 0x%08x\n",
		__func__,(int)ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mutex_lock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	sprd_codec->sprd_linein_headphone_mute = (u32) val;
	if (sprd_codec->sprd_linein_headphone_set) {
		_mixer_linein_headphone_mute_nolock(codec, sprd_codec->sprd_linein_headphone_mute);
	}
	mutex_unlock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	return ret;
}

static int sprd_codec_linein_headphone_mute_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int invert = mc->invert;

	mutex_lock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	ucontrol->value.integer.value[0] = sprd_codec->sprd_linein_headphone_mute;
	mutex_unlock(&sprd_codec->sprd_linein_speaker_mute_mutex);
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}
	return 0;
}

static int sprd_codec_inter_hp_pa_put(struct snd_kcontrol *kcontrol,
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
	struct sprd_codec_inter_hp_pa *p_setting =
	    &sprd_codec->inter_hp_pa.setting;

	sp_asoc_pr_info("Config inter HP PA 0x%08x\n",
			(int)ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mutex_lock(&sprd_codec->inter_hp_pa_mutex);
	sprd_codec->inter_hp_pa.value = (u32) val;
	if (sprd_codec->inter_hp_pa.set) {
		mutex_unlock(&sprd_codec->inter_hp_pa_mutex);
		sprd_codec_hp_pa_lpw(codec, p_setting->class_g_low_power);
		sprd_codec_hp_pa_mode(codec, p_setting->class_g_mode);
		sprd_codec_hp_pa_osc(codec, p_setting->class_g_osc);
		sprd_codec_hp_pa_cgcal_en(codec, p_setting->class_g_cgcal);
	} else {
		mutex_unlock(&sprd_codec->inter_hp_pa_mutex);
	}
	return ret;
}

static int sprd_codec_inter_hp_pa_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int invert = mc->invert;

	mutex_lock(&sprd_codec->inter_hp_pa_mutex);
	ucontrol->value.integer.value[0] = sprd_codec->inter_hp_pa.value;
	mutex_unlock(&sprd_codec->inter_hp_pa_mutex);
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
	int id = GET_MIC_BIAS_ID(reg);

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
	int id = GET_MIC_BIAS_ID(reg);

	ucontrol->value.integer.value[0] = sprd_codec->mic_bias[id];
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(adc_tlv, -600, 75, 0);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -3600, 300, 1);
static const DECLARE_TLV_DB_SCALE(ear_tlv, -3600, 300, 1);
static const DECLARE_TLV_DB_SCALE(spk_tlv, -2400, 300, 1);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -350, 50, 1);
static const DECLARE_TLV_DB_SCALE(mic_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(auxmic_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(headmic_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(ailr_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_RANGE(clsg_1_tlv,
	0, 7, TLV_DB_SCALE_ITEM(-3000, 300, 1),
	8, 46, TLV_DB_SCALE_ITEM(-850, 50, 0),
);

static const struct soc_enum codec_info_enum =
	SOC_ENUM_SINGLE_EXT(SP_AUDIO_CODEC_NUM, codec_hw_info);

static const DECLARE_TLV_DB_SCALE(clsg_2_tlv, -84, 12, 0);


#define SPRD_CODEC_PGA(xname, xreg, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, FUN_REG(xreg), 0, 15, 0, \
			sprd_codec_vol_get, sprd_codec_vol_put, tlv_array)

#define SPRD_CODEC_PGA_MAX(xname, xreg, max, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, FUN_REG(xreg), 0, max, 0, \
			sprd_codec_vol_get, sprd_codec_vol_put, tlv_array)

#define SPRD_CODEC_MIC_BIAS(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, \
			sprd_codec_mic_bias_get, sprd_codec_mic_bias_put)

static const struct snd_kcontrol_new sprd_codec_snd_controls[] = {
	SPRD_CODEC_PGA("SPKL Playback Volume", SPRD_CODEC_PGA_SPKL, spk_tlv),
	SPRD_CODEC_PGA("SPKR Playback Volume", SPRD_CODEC_PGA_SPKR, spk_tlv),
	SPRD_CODEC_PGA("HPL Playback Volume", SPRD_CODEC_PGA_HPL, hp_tlv),
	SPRD_CODEC_PGA("HPR Playback Volume", SPRD_CODEC_PGA_HPR, hp_tlv),
	SPRD_CODEC_PGA_MAX("HPL CG Playback Volume1", SPRD_CODEC_PGA_CG_HPL_1, 63,
				clsg_1_tlv),
	SPRD_CODEC_PGA_MAX("HPR CG Playback Volume1", SPRD_CODEC_PGA_CG_HPR_1, 63,
				clsg_1_tlv),
	SPRD_CODEC_PGA_MAX("HPL CG Playback Volume2", SPRD_CODEC_PGA_CG_HPL_2, 7,
				clsg_2_tlv),
	SPRD_CODEC_PGA_MAX("HPR CG Playback Volume2", SPRD_CODEC_PGA_CG_HPR_2, 7,
				clsg_2_tlv),
	SPRD_CODEC_PGA("EAR Playback Volume", SPRD_CODEC_PGA_EAR, ear_tlv),

	SPRD_CODEC_PGA_MAX("ADCL Capture Volume", SPRD_CODEC_PGA_ADCL, 63,
			   adc_tlv),
	SPRD_CODEC_PGA_MAX("ADCR Capture Volume", SPRD_CODEC_PGA_ADCR, 63,
			   adc_tlv),

	SPRD_CODEC_PGA_MAX("DACL Playback Volume", SPRD_CODEC_PGA_DACL, 7,
			   dac_tlv),
	SPRD_CODEC_PGA_MAX("DACR Playback Volume", SPRD_CODEC_PGA_DACR, 7,
			   dac_tlv),
	SPRD_CODEC_PGA_MAX("MIC Boost", SPRD_CODEC_PGA_MIC, 3, mic_tlv),
	SPRD_CODEC_PGA_MAX("AUXMIC Boost", SPRD_CODEC_PGA_AUXMIC, 3,
			   auxmic_tlv),
	SPRD_CODEC_PGA_MAX("HEADMIC Boost", SPRD_CODEC_PGA_HEADMIC, 3,
			   headmic_tlv),
	SPRD_CODEC_PGA_MAX("Linein Boost", SPRD_CODEC_PGA_AIL, 3, ailr_tlv),

	SOC_SINGLE_EXT("Inter PA Config", 0, 0, LONG_MAX, 0,
		       sprd_codec_inter_pa_get, sprd_codec_inter_pa_put),

	SOC_SINGLE_EXT("Inter HP PA Config", 0, 0, LONG_MAX, 0,
		       sprd_codec_inter_hp_pa_get, sprd_codec_inter_hp_pa_put),

	SPRD_CODEC_MIC_BIAS("MIC Bias Switch", SPRD_CODEC_MIC_BIAS),

	SPRD_CODEC_MIC_BIAS("AUXMIC Bias Switch", SPRD_CODEC_AUXMIC_BIAS),

	SPRD_CODEC_MIC_BIAS("HEADMIC Bias Switch", SPRD_CODEC_HEADMIC_BIAS),

	SOC_SINGLE_EXT("Linein Mute Switch", 0, 0, 1, 0,
		       sprd_codec_linein_mute_get, sprd_codec_linein_mute_put),
	SOC_SINGLE_EXT("Linein Speaker Mute Switch", 0, 0, 1, 0,
		       sprd_codec_linein_speaker_mute_get, sprd_codec_linein_speaker_mute_put),
	SOC_SINGLE_EXT("Linein Headphone Mute Switch", 0, 0, 1, 0,
		       sprd_codec_linein_headphone_mute_get, sprd_codec_linein_headphone_mute_put),

	SOC_ENUM_EXT("Aud Codec Info", codec_info_enum,
			   sprd_codec_info_get, sprd_codec_info_put),
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
		int id = GET_MIXER_ID(FUN_REG(reg));
		struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
		return mixer->on;
	} else if (IS_SPRD_CODEC_PGA_RANG(FUN_REG(reg))) {
	} else if (IS_SPRD_CODEC_MIC_BIAS_RANG(FUN_REG(reg))) {
	} else
		sp_asoc_pr_dbg("read the register is not codec's reg = 0x%x\n",
			       reg);
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
	} else if (IS_SPRD_CODEC_MIXER_RANG(FUN_REG(reg))) {
		struct sprd_codec_priv *sprd_codec =
		    snd_soc_codec_get_drvdata(codec);
		int id = GET_MIXER_ID(FUN_REG(reg));
		struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
		mixer->on = val ? 1 : 0;
	} else if (IS_SPRD_CODEC_PGA_RANG(FUN_REG(reg))) {
	} else if (IS_SPRD_CODEC_MIC_BIAS_RANG(FUN_REG(reg))) {
	} else
		sp_asoc_pr_dbg("write the register is not codec's reg = 0x%x\n",
			       reg);
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
		sp_asoc_pr_info("Capture rate is [%d]\n", rate);
		if (dai->id != SPRD_CODEC_IIS1_ID) {
			sprd_codec->ad_sample_val = rate;
			sprd_codec_set_ad_sample_rate(codec, rate, mask, shift);
		} else {
			sprd_codec->ad1_sample_val = rate;
			sprd_codec_set_ad_sample_rate(codec, rate,
						      ADC1_SRC_N_MASK,
						      ADC1_SRC_N);
		}
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
	if (dai->id == SPRD_CODEC_IIS1_ID)	/*codec_dai_1 has no playback */
		return 0;

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
	 SRC_SUPPORT_RATE  | 	\
	 SNDRV_PCM_RATE_48000)

/* PCM Playing and Recording default in full duplex mode */
static struct snd_soc_dai_driver sprd_codec_dai[] = {
	{
	 .name = "sprd-codec-i2s",
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
	 .name = "sprd-codec-vaudio",
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
	/*digital  ad1 */
	{
	 .id = SPRD_CODEC_IIS1_ID,
	 .name = "codec-i2s-ext",
	 .capture = {
		     .stream_name = "Ext-Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SPRD_CODEC_PCM_AD_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &sprd_codec_dai_ops,
	 },
#ifdef CONFIG_SND_SOC_SPRD_VAUDIO
	/*digital  ad1 */
	{
	 .id = SPRD_CODEC_IIS1_ID,
	 .name = "codec-vaudio-ext",
	 .capture = {
		     .stream_name = "Ext-Capture-Vaudio",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SPRD_CODEC_PCM_AD_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &sprd_codec_dai_ops,
	 },
#endif
	{
	 .name = "sprd-codec-fm",
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
#ifndef  CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
	int hp_plug_state = get_hp_plug_state();
#endif

	sp_asoc_pr_dbg("%s\n", __func__);

	codec->dapm.idle_bias_off = 1;

	sprd_codec->codec = codec;

	sprd_codec_proc_init(sprd_codec);

	sprd_codec_audio_ldo(sprd_codec);
#ifdef  CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
	sprd_inter_headphone_pa_pre(sprd_codec, 1);
#else
	sprd_inter_headphone_pa_pre(sprd_codec, hp_plug_state);
#endif
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
	sprd_codec_power_get(dev, &sprd_codec->head_mic, "HEADMICBIAS");
	sprd_codec_power_get(dev, &sprd_codec->cg_ldo, "CGLDO");

	return 0;
}

static void sprd_codec_power_regulator_exit(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec_power_put(&sprd_codec->main_mic);
	sprd_codec_power_put(&sprd_codec->aux_mic);
	sprd_codec_power_put(&sprd_codec->head_mic);
	sprd_codec_power_put(&sprd_codec->cg_ldo);
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
	u32 ana_chip_id;

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
		if (!of_property_read_u32(node, "sprd,hp_pa_ver", &val)) {
			if (val < SPRD_CODEC_HP_PA_VER_MAX) {
				sprd_codec->hp_ver = val;
				sp_asoc_pr_dbg("Set HP PA Ver is %d!\n", val);
			} else {
				pr_err
				    ("ERR:This driver just support less %d version!\n",
				     SPRD_CODEC_HP_PA_VER_MAX);
				return -EINVAL;
			}
		}
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
	ana_chip_id  = sci_get_ana_chip_id() >> 16;
	if (ana_chip_id == 0x2713)
		sprd_codec->hp_ver = SPRD_CODEC_HP_PA_VER_1;
	else if (ana_chip_id == 0x2711)
		sprd_codec->hp_ver = SPRD_CODEC_HP_PA_VER_2;
	else {
		pr_info("ana_chip_id is 0x%x\n", (unsigned int)sci_get_ana_chip_id);
	}
	platform_set_drvdata(pdev, sprd_codec);

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
	sprd_codec_inter_hp_pa_init(sprd_codec);
	sprd_codec_linein_mute_init(sprd_codec);

	sprd_codec_power_regulator_init(sprd_codec, &pdev->dev);

	mutex_init(&sprd_codec->inter_pa_mutex);
	mutex_init(&sprd_codec->inter_hp_pa_mutex);
	spin_lock_init(&sprd_codec->sprd_codec_fun_lock);
	spin_lock_init(&sprd_codec->sprd_codec_pa_sw_lock);

	sprd_codec_vcom_ldo_cfg(sprd_codec, ldo_v_map, ARRAY_SIZE(ldo_v_map));
	sprd_codec_pa_ldo_cfg(sprd_codec, ldo_v_map, ARRAY_SIZE(ldo_v_map));

	sprd_codec->nb.notifier_call = hp_notifier_handler;
#ifndef  CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
	hp_register_notifier(&sprd_codec->nb);
#endif

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
#ifndef  CONFIG_SND_SOC_SPRD_USE_EAR_JACK_TYPE13
	hp_unregister_notifier(&sprd_codec->nb);
#endif
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id codec_of_match[] = {
	{.compatible = "sprd,sprd-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, codec_of_match);
#endif

static struct platform_driver sprd_codec_codec_driver = {
	.driver = {
		   .name = "sprd-codec",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(codec_of_match),
		   },
	.probe = sprd_codec_probe,
	.remove = sprd_codec_remove,
};

module_platform_driver(sprd_codec_codec_driver);

MODULE_DESCRIPTION("SPRD-CODEC ALSA SoC codec driver");
MODULE_AUTHOR("Jian Chen <jian.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:sprd-codec");
