/*
 * audio.h: Audio amplifier driver for D2199
 *
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 * Author: Mariusz Wojtasik <mariusz.wojtasik@diasemi.com>
 *
 * This program is free software; you can redistribute  it and/or modify
 * it under  the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 *
 */

#ifndef __LINUX_BOBCAT_AUDIO_H
#define __LINUX_BOBCAT_AUDIO_H

#include <linux/platform_device.h>

#define BOBCAT_MAX_GAIN_TABLE 24

enum d2199_audio_input_val {
	BOBCAT_IN_NONE   = 0x00,
	BOBCAT_IN_A1     = 0x01,
	BOBCAT_IN_A2     = 0x02,
	BOBCAT_IN_B1     = 0x04,
	BOBCAT_IN_B2     = 0x08
};

enum d2199_input_mode_val {
	BOBCAT_IM_TWO_SINGLE_ENDED   = 0x00,
	BOBCAT_IM_ONE_SINGLE_ENDED_1 = 0x01,
	BOBCAT_IM_ONE_SINGLE_ENDED_2 = 0x02,
	BOBCAT_IM_FULLY_DIFFERENTIAL = 0x03
};
enum d2199_input_path_sel {
	BOBCAT_INPUTA,
	BOBCAT_INPUTB
};

enum d2199_preamp_gain_val {
	BOBCAT_PREAMP_GAIN_NEG_6DB   = 0x07,
	BOBCAT_PREAMP_GAIN_NEG_5DB   = 0x08,
	BOBCAT_PREAMP_GAIN_NEG_4DB   = 0x09,
	BOBCAT_PREAMP_GAIN_NEG_3DB   = 0x0a,
	BOBCAT_PREAMP_GAIN_NEG_2DB   = 0x0b,
	BOBCAT_PREAMP_GAIN_NEG_1DB   = 0x0c,
	BOBCAT_PREAMP_GAIN_0DB       = 0x0d,
	BOBCAT_PREAMP_GAIN_1DB       = 0x0e,
	BOBCAT_PREAMP_GAIN_2DB       = 0x0f,
	BOBCAT_PREAMP_GAIN_3DB       = 0x10,
	BOBCAT_PREAMP_GAIN_4DB       = 0x11,
	BOBCAT_PREAMP_GAIN_5DB       = 0x12,
	BOBCAT_PREAMP_GAIN_6DB       = 0x13,
	BOBCAT_PREAMP_GAIN_7DB       = 0x14,
	BOBCAT_PREAMP_GAIN_8DB       = 0x15,
	BOBCAT_PREAMP_GAIN_9DB       = 0x16,
	BOBCAT_PREAMP_GAIN_10DB      = 0x17,
	BOBCAT_PREAMP_GAIN_11DB      = 0x18,
	BOBCAT_PREAMP_GAIN_12DB      = 0x19,
	BOBCAT_PREAMP_GAIN_13DB      = 0x1a,
	BOBCAT_PREAMP_GAIN_14DB      = 0x1b,
	BOBCAT_PREAMP_GAIN_15DB      = 0x1c,
	BOBCAT_PREAMP_GAIN_16DB      = 0x1d,
	BOBCAT_PREAMP_GAIN_17DB      = 0x1e,
	BOBCAT_PREAMP_GAIN_18DB      = 0x1f,
	BOBCAT_PREAMP_GAIN_MUTE
};

static inline int d2199_pagain_to_reg(enum d2199_preamp_gain_val gain_val)
{
	return (int)gain_val;
}

static inline enum d2199_preamp_gain_val d2199_reg_to_pagain(int reg)
{
	return ((reg < BOBCAT_PREAMP_GAIN_NEG_6DB) ? BOBCAT_PREAMP_GAIN_NEG_6DB :
			(enum d2199_preamp_gain_val)reg);
}

static inline int d2199_pagain_to_db(enum d2199_preamp_gain_val gain_val)
{
	return (int)gain_val - (int)BOBCAT_PREAMP_GAIN_0DB;
}

static inline enum d2199_preamp_gain_val d2199_db_to_pagain(int db)
{
	return (enum d2199_preamp_gain_val)(db + (int)BOBCAT_PREAMP_GAIN_0DB);
}

enum d2199_mixer_selector_val {
	BOBCAT_MSEL_A1   = 0x01, /* if A is fully diff. then selects A2 as the inverting input */
	BOBCAT_MSEL_B1   = 0x02, /* if B is fully diff. then selects B2 as the inverting input */
	BOBCAT_MSEL_A2   = 0x04, /* if A is fully diff. then selects A1 as the non-inverting input */
	BOBCAT_MSEL_B2   = 0x08  /* if B is fully diff. then selects B1 as the non-inverting input */
};

enum d2199_mixer_attenuation_val {
	BOBCAT_MIX_ATT_0DB       = 0x00, /* attenuation = 0dB */
	BOBCAT_MIX_ATT_NEG_6DB   = 0x01, /* attenuation = -6dB */
	BOBCAT_MIX_ATT_NEG_9DB5  = 0x02, /* attenuation = -9.5dB */
	BOBCAT_MIX_ATT_NEG_12DB  = 0x03  /* attenuation = -12dB */
};

enum d2199_hp_vol_val {
	BOBCAT_HPVOL_NEG_57DB,   BOBCAT_HPVOL_NEG_56DB,   BOBCAT_HPVOL_NEG_55DB,   BOBCAT_HPVOL_NEG_54DB,
	BOBCAT_HPVOL_NEG_53DB,   BOBCAT_HPVOL_NEG_52DB,   BOBCAT_HPVOL_NEG_51DB,   BOBCAT_HPVOL_NEG_50DB,
	BOBCAT_HPVOL_NEG_49DB,   BOBCAT_HPVOL_NEG_48DB,   BOBCAT_HPVOL_NEG_47DB,   BOBCAT_HPVOL_NEG_46DB,
	BOBCAT_HPVOL_NEG_45DB,   BOBCAT_HPVOL_NEG_44DB,   BOBCAT_HPVOL_NEG_43DB,   BOBCAT_HPVOL_NEG_42DB,
	BOBCAT_HPVOL_NEG_41DB,   BOBCAT_HPVOL_NEG_40DB,   BOBCAT_HPVOL_NEG_39DB,   BOBCAT_HPVOL_NEG_38DB,
	BOBCAT_HPVOL_NEG_37DB,   BOBCAT_HPVOL_NEG_36DB,   BOBCAT_HPVOL_NEG_35DB,   BOBCAT_HPVOL_NEG_34DB,
	BOBCAT_HPVOL_NEG_33DB,   BOBCAT_HPVOL_NEG_32DB,   BOBCAT_HPVOL_NEG_31DB,   BOBCAT_HPVOL_NEG_30DB,
	BOBCAT_HPVOL_NEG_29DB,   BOBCAT_HPVOL_NEG_28DB,   BOBCAT_HPVOL_NEG_27DB,   BOBCAT_HPVOL_NEG_26DB,
	BOBCAT_HPVOL_NEG_25DB,   BOBCAT_HPVOL_NEG_24DB,   BOBCAT_HPVOL_NEG_23DB,   BOBCAT_HPVOL_NEG_22DB,
	BOBCAT_HPVOL_NEG_21DB,   BOBCAT_HPVOL_NEG_20DB,   BOBCAT_HPVOL_NEG_19DB,   BOBCAT_HPVOL_NEG_18DB,
	BOBCAT_HPVOL_NEG_17DB,   BOBCAT_HPVOL_NEG_16DB,   BOBCAT_HPVOL_NEG_15DB,   BOBCAT_HPVOL_NEG_14DB,
	BOBCAT_HPVOL_NEG_13DB,   BOBCAT_HPVOL_NEG_12DB,   BOBCAT_HPVOL_NEG_11DB,   BOBCAT_HPVOL_NEG_10DB,
	BOBCAT_HPVOL_NEG_9DB,    BOBCAT_HPVOL_NEG_8DB,    BOBCAT_HPVOL_NEG_7DB,    BOBCAT_HPVOL_NEG_6DB,
	BOBCAT_HPVOL_NEG_5DB,    BOBCAT_HPVOL_NEG_4DB,    BOBCAT_HPVOL_NEG_3DB,    BOBCAT_HPVOL_NEG_2DB,
	BOBCAT_HPVOL_NEG_1DB,    BOBCAT_HPVOL_0DB,        BOBCAT_HPVOL_1DB,        BOBCAT_HPVOL_2DB,
	BOBCAT_HPVOL_3DB,        BOBCAT_HPVOL_4DB,        BOBCAT_HPVOL_5DB,        BOBCAT_HPVOL_6DB,
	BOBCAT_HPVOL_MUTE
};


#define PMU_HSGAIN_NUM 	BOBCAT_HPVOL_MUTE
#define PMU_IHFGAIN_NUM	BOBCAT_SPVOL_MUTE


static inline int d2199_hpvol_to_reg(enum d2199_hp_vol_val vol_val)
{
	return (int)vol_val;
}

static inline enum d2199_hp_vol_val d2199_reg_to_hpvol(int reg)
{
	return (enum d2199_hp_vol_val)reg;
}

static inline int d2199_hpvol_to_db(enum d2199_hp_vol_val vol_val)
{
	return (int)vol_val - (int)BOBCAT_HPVOL_0DB;
}

static inline enum d2199_hp_vol_val d2199_db_to_hpvol(int db)
{
	return (enum d2199_hp_vol_val)(db + (int)BOBCAT_HPVOL_0DB);
}

enum d2199_sp_vol_val {
	BOBCAT_SPVOL_NEG_24DB    = 0x1b, BOBCAT_SPVOL_NEG_23DB = 0x1c,
	BOBCAT_SPVOL_NEG_22DB    = 0x1d, BOBCAT_SPVOL_NEG_21DB = 0x1e,
	BOBCAT_SPVOL_NEG_20DB    = 0x1f, BOBCAT_SPVOL_NEG_19DB = 0x20,
	BOBCAT_SPVOL_NEG_18DB    = 0x21, BOBCAT_SPVOL_NEG_17DB = 0x22,
	BOBCAT_SPVOL_NEG_16DB    = 0x23, BOBCAT_SPVOL_NEG_15DB = 0x24,
	BOBCAT_SPVOL_NEG_14DB    = 0x25, BOBCAT_SPVOL_NEG_13DB = 0x26,
	BOBCAT_SPVOL_NEG_12DB    = 0x27, BOBCAT_SPVOL_NEG_11DB = 0x28,
	BOBCAT_SPVOL_NEG_10DB    = 0x29, BOBCAT_SPVOL_NEG_9DB = 0x2a,
	BOBCAT_SPVOL_NEG_8DB     = 0x2b, BOBCAT_SPVOL_NEG_7DB = 0x2c,
	BOBCAT_SPVOL_NEG_6DB     = 0x2d, BOBCAT_SPVOL_NEG_5DB = 0x2e,
	BOBCAT_SPVOL_NEG_4DB     = 0x2f, BOBCAT_SPVOL_NEG_3DB = 0x30,
	BOBCAT_SPVOL_NEG_2DB     = 0x31, BOBCAT_SPVOL_NEG_1DB = 0x32,
	BOBCAT_SPVOL_0DB         = 0x33,
	BOBCAT_SPVOL_1DB         = 0x34, BOBCAT_SPVOL_2DB = 0x35,
	BOBCAT_SPVOL_3DB         = 0x36, BOBCAT_SPVOL_4DB = 0x37,
	BOBCAT_SPVOL_5DB         = 0x38, BOBCAT_SPVOL_6DB = 0x39,
	BOBCAT_SPVOL_7DB         = 0x3a, BOBCAT_SPVOL_8DB = 0x3b,
	BOBCAT_SPVOL_9DB         = 0x3c, BOBCAT_SPVOL_10DB = 0x3d,
	BOBCAT_SPVOL_11DB        = 0x3e, BOBCAT_SPVOL_12DB = 0x3f,
	BOBCAT_SPVOL_MUTE
};

static inline int d2199_spvol_to_reg(enum d2199_sp_vol_val vol_val)
{
	return (int)vol_val;
}

static inline enum d2199_sp_vol_val d2199_reg_to_spvol(int reg)
{
	return ((reg < BOBCAT_SPVOL_NEG_24DB) ? BOBCAT_SPVOL_NEG_24DB :
			(enum d2199_sp_vol_val)reg);
}

static inline int d2199_spvol_to_db(enum d2199_sp_vol_val vol_val)
{
	return (int)vol_val - (int)BOBCAT_SPVOL_0DB;
}

static inline enum d2199_sp_vol_val d2199_db_to_spvol(int db)
{
	return (enum d2199_sp_vol_val)(db + (int)BOBCAT_SPVOL_0DB);
}


enum d2199_audio_output_sel {
	BOBCAT_OUT_NONE  = 0x00,
	BOBCAT_OUT_HPL   = 0x01,
	BOBCAT_OUT_HPR   = 0x02,
	BOBCAT_OUT_HPLR  = BOBCAT_OUT_HPL | BOBCAT_OUT_HPR,
	BOBCAT_OUT_SPKR  = 0x04,
};

struct d2199_audio_platform_data
{
	u8 ina_def_mode;
	u8 inb_def_mode;
	u8 ina_def_preampgain;
	u8 inb_def_preampgain;

	u8 lhs_def_mixer_in;
	u8 rhs_def_mixer_in;
	u8 ihf_def_mixer_in;

	int hs_input_path;
	int ihf_input_path;
};

struct d2199_audio {
	struct platform_device  *pdev;
	bool IHFenabled;
	bool HSenabled;
	u8 hs_pga_gain;
	u8 hs_pre_gain;
	struct timer_list timer;
	struct work_struct work;
	u8 AudioStart; //0=not started, 1=hs starting, 2=ihf starting, 3=started
};

int d2199_audio_hs_poweron(bool on);
int d2199_audio_hs_shortcircuit_enable(bool en);
int d2199_audio_hs_set_gain(enum d2199_audio_output_sel hs_path_sel, enum d2199_hp_vol_val hs_gain_val);
int d2199_audio_hs_ihf_poweron(void);
int d2199_audio_hs_ihf_poweroff(void);
int d2199_audio_hs_ihf_enable_bypass(bool en);
int d2199_audio_hs_ihf_set_gain(enum d2199_sp_vol_val ihfgain_val);
int d2199_audio_set_mixer_input(enum d2199_audio_output_sel path_sel, enum d2199_audio_input_val input_val);
int d2199_audio_set_input_mode(enum d2199_input_path_sel inpath_sel, enum d2199_input_mode_val mode_val);
int d2199_audio_set_input_preamp_gain(enum d2199_input_path_sel inpath_sel, enum d2199_preamp_gain_val pagain_val);
int d2199_audio_hs_preamp_gain(enum d2199_preamp_gain_val hsgain_val);
int d2199_audio_ihf_preamp_gain(enum d2199_preamp_gain_val ihfgain_val);
int d2199_audio_enable_zcd(int enable);
int d2199_audio_enable_vol_slew(int enable);
int d2199_set_hs_noise_gate(u16 regval);		
int d2199_set_ihf_noise_gate(u16 regval);
int d2199_set_ihf_none_clip(u16 regval);
int d2199_set_ihf_pwr(u8 regval);
int d2199_sp_set_hi_impedance(u8 set_last_bit);

#endif /* __LINUX_BOBCAT_AUDIO_H */
