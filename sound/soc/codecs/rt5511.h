/*
 * rt5511.h  --  RT5511 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _RT5511_H
#define _RT5511_H

#include <sound/soc.h>
#include <linux/firmware.h>
#include <linux/completion.h>
#include <linux/mfd/rt5511/pdata.h>

#include "rt_hubs.h"

/* RT5511 Codec DAI Name */
#define RT5511_CODEC_NAME		"rt5511-codec"
#define RT5511_CODEC_DAI1_NAME		"rt5511-aif1"
#define RT5511_CODEC_DAI2_NAME		"rt5511-aif2"
#define RT5511_CODEC_DAI3_NAME		"rt5511-aif3"

/* Sources for AIF SYSCLK - use with set_dai_sysclk() */

#define RT5511_SYSCLK_PLL		1
#define RT5511_SYSCLK_PLL_DIV2		2
#define RT5511_SYSCLK_MCLK1		3
#define RT5511_SYSCLK_MCLK2		4

/* Source for PLL Frequency - use with set_pll() */

#define RT5511_PLL	0

#define RT5511_PLL_SRC_NULL	0
#define RT5511_PLL_SRC_MCLK1	1
#define RT5511_PLL_SRC_BCLK1	2
#define RT5511_PLL_SRC_LRCK1	3

#define RT5511_PLL_SRC_MCLK2	(4+1)
#define RT5511_PLL_SRC_BCLK2	(4+2)
#define RT5511_PLL_SRC_LRCK2	(4+3)

#define RT5511_CLK_DIV_PLL	1

enum rt5511_vmid_mode {
	RT5511_VMID_NORMAL,
	RT5511_VMID_FORCE,
};

#define RT5511_NUM_DRC 4

#define RT5511_CACHE_SIZE 256

#define RT5511_ANALOG_VOL_SIZE 11

struct rt5511_access_mask {
	char *name;
	u32 readable;   /* Mask of readable bits */
	u32 writable;   /* Mask of writable bits */
	u8 is_volatile;
};


/* Private cache */

extern void rt5511_init_cache(void);
extern int rt5511_reg_cache_sync(struct snd_soc_codec *codec);

extern u32 rt5511_reg_cache[RT5511_CACHE_SIZE];
extern const struct rt5511_access_mask rt5511_access_masks[RT5511_CACHE_SIZE];
extern const u32 rt5511_reg_defaults[RT5511_CACHE_SIZE];
extern const int rt5511_reg_size[RT5511_CACHE_SIZE];

int rt5511_vmid_mode(struct snd_soc_codec *codec, enum rt5511_vmid_mode mode);

int rt5511_aif_ev(struct snd_soc_dapm_widget *w,
		  struct snd_kcontrol *kcontrol, int event);

/* codec private data */
struct rt5511_pll_config {
	int src;
	int in;
	int out;
};

struct rt5511_priv {
	struct rt_hubs_data hubs;
	enum snd_soc_control_type control_type;
	void *control_data;
	struct snd_soc_codec *codec;
	int sysclk;
	int sysclk_rate;
	int real_pll;
	int mclk[2];
	int aifclk[2];
	int aifdiv[2];
	struct rt5511_pll_config pll, pll_suspend;
	struct completion fll_locked[2];
	bool fll_locked_irq;
	bool fll_byp;

	int vmid_refcount;
	int active_refcount;
	enum rt5511_vmid_mode vmid_mode;

	int dac_rates[2];
	int lrclk_shared[2];

	int mbc_ena[3];
	int hpf1_ena[3];
	int hpf2_ena[3];
	int vss_ena[3];
	int enh_eq_ena[3];

	/* Platform dependant DRC configuration */
	const char **drc_texts;
	int drc_cfg[RT5511_NUM_DRC];
	int drc_en[RT5511_DRC_REG_NUM];
	struct soc_enum drc_enum;

	/* Platform dependant ReTune mobile configuration */
	int num_eq_texts;
	const char **eq_texts;
	int eq_cfg[RT5511_NUM_EQ];
	int eq_en[RT5511_BQ_PATH_NUM];
	struct soc_enum eq_enum;

	/* Platform dependant DRE configuration */
	int dre_en;
	u8 dre_cfg[RT5511A_DRE_REG_SIZE];

	/* Platform dependant MBC configuration */
	int mbc_cfg;
	const char **mbc_texts;
	struct soc_enum mbc_enum;

	/* Platform dependant VSS configuration */
	int vss_cfg;
	const char **vss_texts;
	struct soc_enum vss_enum;

	/* Platform dependant VSS HPF configuration */
	int vss_hpf_cfg;
	const char **vss_hpf_texts;
	struct soc_enum vss_hpf_enum;

	/* Platform dependant enhanced EQ configuration */
	int enh_eq_cfg;
	const char **enh_eq_texts;
	struct soc_enum enh_eq_enum;

	struct mutex accdet_lock;

	int src44100;
	int revision;
	struct rt5511_pdata *pdata;

	unsigned int aif1clk_enable:1;
	unsigned int aif2clk_enable:1;

	unsigned int aif1clk_disable:1;
	unsigned int aif2clk_disable:1;

	int dsp_active;
	const struct firmware *cur_fw;
	const struct firmware *mbc;
	const struct firmware *mbc_vss;
	const struct firmware *enh_eq;

	bool aifclk_en[3];
};

#endif
