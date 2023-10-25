// SPDX-License-Identifier: GPL-2.0+
/* fs1818.h -- fs1818 ALSA SoC Audio driver
 *
 * Copyright (C) Fourier Semiconductor Inc. 2016-2021.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FS1818_H__
#define __FS1818_H__

#include <sound/soc.h>
#include <linux/version.h>
#include <linux/regmap.h>

/* VERSION INFORMATION */
#define FSM_CODE_VERSION "v3.0.10"

#define FS1818_STATUS         0xF000
#define FS1818_BOVDS          0x0000
#define FS1818_PLLS           0x0100
#define FS1818_OTDS           0x0200
#define FS1818_OVDS           0x0300
#define FS1818_UVDS           0x0400
#define FS1818_OCDS           0x0500
#define FS1818_CLKS           0x0600
#define FS1818_SPKS           0x0A00
#define FS1818_SPKT           0x0B00
#define FS1818_DEVID          0xF003
#define FS1818_I2SCTRL        0xF004
#define FS1818_I2SF           0x2004
#define FS1818_CHS12          0x1304
#define FS1818_DISP           0x0A04
#define FS1818_I2SDOE         0x0B04
#define FS1818_I2SSR          0x3C04
#define FS1818_AUDIOCTRL      0xF006
#define FS1818_TEMPSEL        0xF008
#define FS1818_SYSCTRL        0xF009
#define FS1818_OTPACC         0xF00B
#define FS1818_CHIPINI        0xF090
#define FS1818_I2SSET         0xF0A0
#define FS1818_LRCLKP         0x05A0
#define FS1818_BCLKP          0x06A0
#define FS1818_AECSELL        0x28A0
#define FS1818_AECSELR        0x2CA0
#define FS1818_DSPCTRL        0xF0A1
#define FS1818_DACCTRL        0xF0AE
#define FS1818_DACMUTE        0x08AE
#define FS1818_TSCTRL         0xF0AF
#define FS1818_MODCTRL        0xF0B0
#define FS1818_DIGSTAT        0xF0BD
#define FS1818_DACRUN         0x01BD
#define FS1818_BSTCTRL        0xF0C0
#define FS1818_DISCHARGE      0x00C0
#define FS1818_DAC_GAIN       0x11C0
#define FS1818_BSTEN          0x03C0
#define FS1818_BST_MODE       0x14C0
#define FS1818_ILIM_SEL       0x36C0
#define FS1818_VOUT_SEL       0x3AC0
#define FS1818_SSEND          0x0FC0
#define FS1818_PLLCTRL1       0xF0C1
#define FS1818_PLLCTRL2       0xF0C2
#define FS1818_PLLCTRL3       0xF0C3
#define FS1818_PLLCTRL4       0xF0C4
#define FS1818_OTCTRL         0xF0C6
#define FS1818_ANACTRL        0xF0D0
#define FS1818_CLDCTRL        0xF0D3
#define FS1818_REGCTRL        0x08D3
#define FS1818_AUXCFG         0xF0D7
#define FS1818_OTPPG1W0       0xF0E4

#define FS18XX_DRV_NAME  "fs18xx"

#define KERNEL_VERSION_HIGHER(a, b, c) \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(a, b, c))

#if KERNEL_VERSION_HIGHER(4, 19, 0)
#define snd_soc_codec              snd_soc_component
#define snd_soc_add_codec_controls snd_soc_add_component_controls
#define snd_soc_register_codec     snd_soc_register_component
#define snd_soc_unregister_codec   snd_soc_unregister_component
#define snd_soc_codec_driver       snd_soc_component_driver
#define snd_soc_codec_get_drvdata  snd_soc_component_get_drvdata
#define snd_soc_codec_get_drvdata  snd_soc_component_get_drvdata
#define snd_soc_kcontrol_codec     snd_soc_kcontrol_component
#define snd_soc_dapm_to_codec      snd_soc_dapm_to_component
#define snd_soc_codec_get_dapm     snd_soc_component_get_dapm
#define snd_soc_get_codec(dai)     (dai->component)
#define remove_ret_type void
#define remove_ret_val
#else
#define snd_soc_get_codec(dai)     (dai->codec)
#define remove_ret_type int
#define remove_ret_val (0)
#endif

#define HIGH8(val)    ((val >> 8) & 0xFF)
#define LOW8(val)     (val & 0xFF)
#define WORD(addr)    ((*(addr) << 8) & *((addr) + 1))
#define REG(bf)       (bf & 0xFF)

#define FSM_I2C_RETRY         (5)
#define FSM_STR_MAX           (128)
#define FSM_DEV_MAX           (4)
#define FSM_ADDR_BASE         (0x34)

#define LOG_BUF_SIZE          (8 * 8 + 1)

enum dev_id_index {
	FS18XM_DEV_ID = 0x06,
	FS18HF_DEV_ID = 0x0B,
	FSM_DEV_ID_MAX,
};

struct reg_unit {
	uint16_t addr : 8;
	uint16_t pos  : 4;
	uint16_t len  : 4;
	uint16_t value;
};

struct fsm_pll_config {
	uint32_t bclk;
	uint16_t c1;
	uint16_t c2;
	uint16_t c3;
};

struct fsm_hw_params {
	unsigned int sample_rate;
	unsigned int freq_bclk;
	unsigned int dai_fmt;
	uint16_t i2s_fmt;
	uint16_t bit_width;
	uint16_t channel;
	uint16_t boost_mode;
	uint16_t dac_gain;
	uint16_t do_type;
	uint16_t do_enable : 1;
};

struct fsm_dev {
	struct i2c_client *i2c;
	struct device *dev;
#ifdef CONFIG_REGMAP
	struct regmap *regmap;
#endif
	struct snd_soc_codec *codec;
	struct workqueue_struct *fsm_wq;
	struct delayed_work monitor_work;
	struct mutex i2c_lock;
	struct class fsm_class;
	struct fsm_hw_params hw_params;
	int irq_id;
	uint16_t bus  : 8;
	uint16_t addr : 8;
	uint16_t version;
	uint16_t revid;
	uint16_t pos_mask : 8; // position
	uint16_t rsvd     : 8;

	uint16_t use_irq   : 1;
	uint16_t dev_init  : 1;
	uint16_t start_up  : 1;
	uint16_t amp_on    : 1;
	uint16_t bulk_wt   : 1;
	uint16_t montr_en  : 1;
	uint8_t montr_pd;
	uint8_t digi_vol;

	int id;
	int errcode;
	char const *chn_name;
};

#endif // __FS1818_H__
