// SPDX-License-Identifier: GPL-2.0-or-later
/* sma1305.c -- sma1305 ALSA SoC Audio driver
 *
 * r012, 2021.03.16	- initial version  sma1305
 *
 * Copyright 2020 Silicon Mitus Corporation / Iron Device Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/version.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <asm/div64.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>

#include "sma1305.h"

#define CHECK_PERIOD_TIME 1 /* sec per HZ */

#define PLL_MATCH(_input_clk_name, _output_clk_name, _input_clk,\
		_post_n, _n, _vco,  _p_cp)\
{\
	.input_clk_name		= _input_clk_name,\
	.output_clk_name	= _output_clk_name,\
	.input_clk		= _input_clk,\
	.post_n			= _post_n,\
	.n			= _n,\
	.vco			= _vco,\
	.p_cp		= _p_cp,\
}

enum sma1305_type {
	SMA1305,
};

/* PLL clock setting Table */
struct sma1305_pll_match {
	char *input_clk_name;
	char *output_clk_name;
	unsigned int input_clk;
	unsigned int post_n;
	unsigned int n;
	unsigned int vco;
	unsigned int p_cp;
};

struct sma1305_priv {
	enum sma1305_type devtype;
	struct attribute_group *attr_grp;
	struct kobject *kobj;
	struct regmap *regmap;
	struct sma1305_pll_match *pll_matches;
	int num_of_pll_matches;
	unsigned int spk_rcv_mode;
	unsigned int sys_clk_id;
	unsigned int init_vol;
	unsigned int cur_vol;
	unsigned int tsdw_cnt;
	unsigned int tdm_slot_rx;
	unsigned int tdm_slot_tx;
	unsigned int bst_vol_lvl_status;
	unsigned int flt_vdd_gain_status;
	unsigned int otp_trm2;
	unsigned int otp_trm3;
	bool sdo_bypass_flag;
	bool amp_power_status;
	bool force_amp_power_down;
	bool stereo_two_chip;
	bool impossible_bst_ctrl;
	long isr_manual_mode;
	struct mutex lock;
	struct delayed_work check_fault_work;
	long check_fault_period;
	long check_fault_status;
	unsigned int format;
	struct device *dev;
	unsigned int rev_num;
	unsigned int last_bclk;
	unsigned int frame_size;
	int irq;
	int gpio_int;
	unsigned int sdo_ch;
	unsigned int sdo0_sel;
	unsigned int sdo1_sel;
	atomic_t irq_enabled;
};

static struct sma1305_pll_match sma1305_pll_matches[] = {
/* in_clk_name, out_clk_name, input_clk post_n, n, vco, p_cp */
PLL_MATCH("1.411MHz",  "24.554MHz", 1411200,  0x06, 0xD1, 0x88, 0x00),
PLL_MATCH("1.536MHz",  "24.576MHz", 1536000,  0x06, 0xC0, 0x88, 0x00),
PLL_MATCH("2.822MHz",  "24.554MHz", 2822400,  0x06, 0xD1, 0x88, 0x04),
PLL_MATCH("3.072MHz",  "24.576MHz", 3072000,  0x06, 0x60, 0x88, 0x00),
PLL_MATCH("6.144MHz",  "24.576MHz", 6144000,  0x06, 0x60, 0x88, 0x04),
PLL_MATCH("12.288MHz", "24.576MHz", 12288000, 0x06, 0x60, 0x88, 0x08),
PLL_MATCH("19.2MHz",   "24.48MHz", 19200000, 0x06, 0x7B, 0x88, 0x0C),
PLL_MATCH("24.576MHz", "24.576MHz", 24576000, 0x06, 0x60, 0x88, 0x0C),
};

static struct snd_soc_component *sma1305_amp_component;

static int sma1305_startup(struct snd_soc_component *);
static int sma1305_shutdown(struct snd_soc_component *);
static int sma1305_spk_rcv_conf(struct snd_soc_component *);

/* Initial register value - 4W SPK 2020.09.25 */
static const struct reg_default sma1305_reg_def[] = {
/*	{ reg,  def },	   Register Name */
	{ 0x00, 0x80 }, /* 0x00 SystemCTRL  */
	{ 0x01, 0x00 }, /* 0x01 InputCTRL1  */
	{ 0x02, 0x64 }, /* 0x02 BrownOut Protection1  */
	{ 0x03, 0x5B }, /* 0x03 BrownOut Protection2  */
	{ 0x04, 0x58 }, /* 0x04 BrownOut Protection3  */
	{ 0x05, 0x52 }, /* 0x05 BrownOut Protection8  */
	{ 0x06, 0x4C }, /* 0x06 BrownOut Protection9  */
	{ 0x07, 0x4A }, /* 0x07 BrownOut Protection10  */
	{ 0x08, 0x47 }, /* 0x08 BrownOut Protection11  */
	{ 0x09, 0x2F }, /* 0x09 OutputCTRL  */
	{ 0x0A, 0x32 }, /* 0x0A SPK_VOL  */
	{ 0x0B, 0x50 }, /* 0x0B BST_TEST  */
	{ 0x0C, 0x8C }, /* 0x0C BST_CTRL8  */
	{ 0x0D, 0x00 }, /* 0x0D SPK_TEST  */
	{ 0x0E, 0x3F }, /* 0x0E MUTE_VOL_CTRL  */
	{ 0x0F, 0x08 }, /* 0x0F VBAT_TEMP_SENSING  */
	{ 0x10, 0x04 }, /* 0x10 SystemCTRL1  */
	{ 0x11, 0x00 }, /* 0x11 SystemCTRL2  */
	{ 0x12, 0x00 }, /* 0x12 SystemCTRL3  */
	{ 0x13, 0x09 }, /* 0x13 Delay  */
	{ 0x14, 0x12 }, /* 0x14 Modulator  */
	{ 0x1C, 0x0F }, /* 0x1C BrownOut Protection20  */
	{ 0x1D, 0x05 }, /* 0x1D BrownOut Protection0  */
	{ 0x1E, 0xA1 }, /* 0x1E Tone Generator  */
	{ 0x1F, 0x67 }, /* 0x1F Tone_Fine volume  */
	{ 0x22, 0x00 }, /* 0x22 Compressor Hysteresis  */
	{ 0x23, 0x1F }, /* 0x23 CompLim1  */
	{ 0x24, 0x7A }, /* 0x24 CompLim2  */
	{ 0x25, 0x00 }, /* 0x25 CompLim3  */
	{ 0x26, 0xFF }, /* 0x26 CompLim4  */
	{ 0x27, 0x1B }, /* 0x27 BrownOut Protection4  */
	{ 0x28, 0x1A }, /* 0x28 BrownOut Protection5  */
	{ 0x29, 0x19 }, /* 0x29 BrownOut Protection12  */
	{ 0x2A, 0x78 }, /* 0x2A BrownOut Protection13  */
	{ 0x2B, 0x97 }, /* 0x2B BrownOut Protection14  */
	{ 0x2C, 0xB6 }, /* 0x2C BrownOut Protection15  */
	{ 0x2D, 0xFF }, /* 0x2D BrownOut Protection6  */
	{ 0x2E, 0xFF }, /* 0x2E BrownOut Protection7  */
	{ 0x2F, 0xFF }, /* 0x2F BrownOut Protection16  */
	{ 0x30, 0xFF }, /* 0x30 BrownOut Protection17  */
	{ 0x31, 0xFF }, /* 0x31 BrownOut Protection18  */
	{ 0x32, 0xFF }, /* 0x32 BrownOut Protection19  */
	{ 0x34, 0x00 }, /* 0x34 OCP_SPK  */
	{ 0x35, 0x16 }, /* 0x35 FDPEC Control0  */
	{ 0x36, 0x92 }, /* 0x36 Protection  */
	{ 0x37, 0x00 }, /* 0x37 SlopeCTRL  */
	{ 0x38, 0x01 }, /* 0x38 Power Meter */
	{ 0x39, 0x10 }, /* 0x39 PMT_NZ_VAL */
	{ 0x3E, 0x01 }, /* 0x3E IDLE_MODE_CTRL */
	{ 0x3F, 0x08 }, /* 0x3F ATEST2  */
	{ 0x8B, 0x06 }, /* 0x8B PLL_POST_N  */
	{ 0x8C, 0xC0 }, /* 0x8C PLL_N  */
	{ 0x8D, 0x88 }, /* 0x8D PLL_A_SETTING  */
	{ 0x8E, 0x00 }, /* 0x8E PLL_P_CP  */
	{ 0x8F, 0x02 }, /* 0x8F Analog Test  */
	{ 0x90, 0x02 }, /* 0x90 CrestLim1  */
	{ 0x91, 0x83 }, /* 0x91 CrestLIm2  */
	{ 0x92, 0xB0 }, /* 0x92 FDPEC Control1  */
	{ 0x93, 0x00 }, /* 0x93 INT Control  */
	{ 0x94, 0xA4 }, /* 0x94 Boost Control9  */
	{ 0x95, 0x54 }, /* 0x95 Boost Control10  */
	{ 0x96, 0x57 }, /* 0x96 Boost Control11  */
	{ 0xA2, 0xCC }, /* 0xA2 TOP_MAN1  */
	{ 0xA3, 0x28 }, /* 0xA3 TOP_MAN2  */
	{ 0xA4, 0x40 }, /* 0xA4 TOP_MAN3  */
	{ 0xA5, 0x01 }, /* 0xA5 TDM1  */
	{ 0xA6, 0x41 }, /* 0xA6 TDM2  */
	{ 0xA7, 0x08 }, /* 0xA7 CLK_MON  */
	{ 0xA8, 0x04 }, /* 0xA8 Boost Control1  */
	{ 0xA9, 0x29 }, /* 0xA9 Boost Control2  */
	{ 0xAA, 0x10 }, /* 0xAA Boost Control3  */
	{ 0xAB, 0x11 }, /* 0xAB Boost Control4  */
	{ 0xAC, 0x10 }, /* 0xAC Boost Control5  */
	{ 0xAD, 0x0F }, /* 0xAD Boost Control6  */
	{ 0xAE, 0xCD }, /* 0xAE Boost Control7  */
	{ 0xAF, 0x41 }, /* 0xAF LPF  */
	{ 0xB0, 0x03 }, /* 0xB0 RMS_TC1  */
	{ 0xB1, 0xEF }, /* 0xB1 RMS_TC2  */
	{ 0xB2, 0x03 }, /* 0xB2 AVG_TC1  */
	{ 0xB3, 0xEF }, /* 0xB3 AVG_TC2  */
	{ 0xB4, 0xF3 }, /* 0xB4 PR_VALUE1  */
	{ 0xB5, 0x3D }, /* 0xB5 PR_VALUE2  */
};

static bool sma1305_readable_register(struct device *dev, unsigned int reg)
{
	bool result;

	if (reg > SMA1305_FF_DEVICE_INDEX)
		return false;

	switch (reg) {
	case SMA1305_00_SYSTEM_CTRL ... SMA1305_1F_TONE_FINE_VOLUME:
	case SMA1305_22_COMP_HYS_SEL ... SMA1305_32_BROWN_OUT_PROT19:
	case SMA1305_34_OCP_SPK ... SMA1305_39_PMT_NZ_VAL:
	case SMA1305_3B_TEST1 ... SMA1305_3F_ATEST2:
	case SMA1305_8B_PLL_POST_N ... SMA1305_9A_OTP_TRM3:
	case SMA1305_A0_PAD_CTRL0 ... SMA1305_B5_PRVALUE2:
	case SMA1305_F7_READY_FOR_T_SAR ... SMA1305_FD_STATUS4:
		result = true;
		break;
	case SMA1305_F5_READY_FOR_V_SAR:
		result = true;
		break;
	case SMA1305_FF_DEVICE_INDEX:
		result = true;
		break;
	default:
		result = false;
	}
	return result;
}

static bool sma1305_writeable_register(struct device *dev, unsigned int reg)
{
	bool result;

	if (reg > SMA1305_FF_DEVICE_INDEX)
		return false;

	switch (reg) {
	case SMA1305_00_SYSTEM_CTRL ... SMA1305_1F_TONE_FINE_VOLUME:
	case SMA1305_22_COMP_HYS_SEL ... SMA1305_32_BROWN_OUT_PROT19:
	case SMA1305_34_OCP_SPK ... SMA1305_39_PMT_NZ_VAL:
	case SMA1305_3B_TEST1 ... SMA1305_3F_ATEST2:
	case SMA1305_8B_PLL_POST_N ... SMA1305_9A_OTP_TRM3:
	case SMA1305_A0_PAD_CTRL0 ... SMA1305_B5_PRVALUE2:
		result = true;
		break;
	default:
		result = false;
	}
	return result;
}

static bool sma1305_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SMA1305_FA_STATUS1 ... SMA1305_FB_STATUS2:
	case SMA1305_FF_DEVICE_INDEX:
		return true;
	default:
		return false;
	}
}

/* DB scale conversion of speaker volume */
static const DECLARE_TLV_DB_SCALE(sma1305_spk_tlv, -6000, 50, 0);

/* common bytes ext functions */
static int bytes_ext_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol, int reg)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	unsigned int i, reg_val;
	u8 *val;

	val = (u8 *)ucontrol->value.bytes.data;
	for (i = 0; i < params->max; i++) {
		regmap_read(sma1305->regmap, reg + i, &reg_val);
		if (sizeof(reg_val) > 2)
			reg_val = cpu_to_le32(reg_val);
		else
			reg_val = cpu_to_le16(reg_val);
		memcpy(val + i, &reg_val, sizeof(u8));
	}

	return 0;
}

static int bytes_ext_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol, int reg)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	u8 *val;
	int i, ret;

	data = kmemdup(ucontrol->value.bytes.data,
			params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *)data;
	for (i = 0; i < params->max; i++) {
		ret = regmap_write(sma1305->regmap, reg + i, *(val + i));
		if (ret) {
			dev_err(component->dev,
				"configuration fail, register: %x ret: %d\n",
				reg + i, ret);
			kfree(data);
			return ret;
		}
	}
	kfree(data);

	return 0;
}

static int power_up_down_control_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long) sma1305->amp_power_status;

	return 0;
}

static int power_up_down_control_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	if (sel && !(sma1305->force_amp_power_down))
		sma1305_startup(component);
	else
		sma1305_shutdown(component);

	return 0;
}

static int power_down_control_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long) sma1305->force_amp_power_down;

	return 0;
}

static int power_down_control_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	sma1305->force_amp_power_down = (bool) ucontrol->value.integer.value[0];

	if (sma1305->force_amp_power_down) {
		dev_info(component->dev, "%s\n", "Force AMP Power Down");
		sma1305_shutdown(component);
	}

	return 0;
}
static int force_sdo_bypass_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long) sma1305->sdo_bypass_flag;

	return 0;
}

static int force_sdo_bypass_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	sma1305->sdo_bypass_flag = (bool)sel;

	return 0;
}

/* 0x01[6:4] I2SMODE */
static const char * const sma1305_input_format_text[] = {
	"I2S", "LJ", "Reserved", "Reserved",
	"RJ_16", "RJ_18", "RJ_20", "RJ_24"};

static const struct soc_enum sma1305_input_format_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_input_format_text),
		sma1305_input_format_text);

static int sma1305_input_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_01_INPUT_CTRL1, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x70) >> 4);

	return 0;
}

static int sma1305_input_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_01_INPUT_CTRL1, 0x70, (sel << 4));

	return 0;
}

/* 0x02~0x08 BO_LVL 1~8 */
static int bo_lvl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_02_BROWN_OUT_PROT1);
}

static int bo_lvl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_02_BROWN_OUT_PROT1);
}

/* 0x09[7:6] PORT_CONFIG */
static const char * const sma1305_port_config_text[] = {
	"IN_Port", "Reserved", "OUT_Port", "Reserved"};

static const struct soc_enum sma1305_port_config_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_port_config_text),
			sma1305_port_config_text);

static int sma1305_port_config_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_09_OUTPUT_CTRL, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_port_config_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_09_OUTPUT_CTRL, 0xC0, (sel << 6));

	return 0;
}

/* 0x09[5:3] SDO_OUT1_SEL */
static const char * const sma1305_sdo_out1_sel_text[] = {
	"Disable", "Format_C", "Mixer_out", "After_DSP",
	"VRMS2_AVG", "Battery", "Temp", "After_Delay"};

static const struct soc_enum sma1305_sdo_out1_sel_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_sdo_out1_sel_text),
	sma1305_sdo_out1_sel_text);

static int sma1305_sdo_out1_sel_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_09_OUTPUT_CTRL, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x38) >> 3);

	return 0;
}

static int sma1305_sdo_out1_sel_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_09_OUTPUT_CTRL, 0x38, (sel << 3));
	return 0;
}

/* 0x09[2:0] SDO_OUT0_SEL */
static const char * const sma1305_sdo_out0_sel_text[] = {
	"Disable", "Format_C", "Mixer_out", "After_DSP",
	"VRMS2_AVG", "Battery", "Temp", "After_Delay"};

static const struct soc_enum sma1305_sdo_out0_sel_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_sdo_out0_sel_text),
	sma1305_sdo_out0_sel_text);

static int sma1305_sdo_out0_sel_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_09_OUTPUT_CTRL, &val);
	ucontrol->value.integer.value[0] = val & 0x07;

	return 0;
}

static int sma1305_sdo_out0_sel_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_09_OUTPUT_CTRL, 0x07, sel);
	return 0;
}

/* 0x0C[7:6] SET_OCP_H */
static const char * const sma1305_set_ocp_h_text[] = {
	"OCP_4.5", "OCP_3.2", "OCP_2.1", "OCP_0.9"};

static const struct soc_enum sma1305_set_ocp_h_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_ocp_h_text),
	sma1305_set_ocp_h_text);

static int sma1305_set_ocp_h_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_0C_BOOST_CTRL8, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_set_ocp_h_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_0C_BOOST_CTRL8, 0xC0, (sel << 6));

	return 0;
}

/* 0x0E[7:6] VOL_SLOPE */
static const char * const sma1305_vol_slope_text[] = {
	"Off", "Slow(21.3ms)", "Medium(10.7ms)",
	"Fast(2.6ms)"};

static const struct soc_enum sma1305_vol_slope_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_vol_slope_text),
	sma1305_vol_slope_text);

static int sma1305_vol_slope_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_0E_MUTE_VOL_CTRL, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_vol_slope_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_0E_MUTE_VOL_CTRL, 0xC0, (sel << 6));

	return 0;
}

/* 0x0E[5:4] MUTE_SLOPE */
static const char * const sma1305_mute_slope_text[] = {
	"Off", "Slow(200ms)", "Medium(50ms)",
	"Fast(10ms)"};

static const struct soc_enum sma1305_mute_slope_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_mute_slope_text),
	sma1305_mute_slope_text);

static int sma1305_mute_slope_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_0E_MUTE_VOL_CTRL, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x30) >> 4);

	return 0;
}

static int sma1305_mute_slope_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_0E_MUTE_VOL_CTRL, 0x30, (sel << 4));

	return 0;
}

/* 0x0F[6:5] VBAT_LPF_BYP */
static const char * const sma1305_vbat_lpf_byp_text[] = {
	"Activate", "Reserved", "Reserved",
	"Bypass"};

static const struct soc_enum sma1305_vbat_lpf_byp_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_vbat_lpf_byp_text),
	sma1305_vbat_lpf_byp_text);

static int sma1305_vbat_lpf_byp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_0F_VBAT_TEMP_SENSING, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x60) >> 5);

	return 0;
}

static int sma1305_vbat_lpf_byp_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_0F_VBAT_TEMP_SENSING, 0x60, (sel << 5));

	return 0;
}

/* 0x0F[1:0] CLK_FREQUENCY */
static const char * const sma1305_clk_frequency_text[] = {
	"Activate", "Reserved", "Reserved",
	"Bypass"};

static const struct soc_enum sma1305_clk_frequency_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_clk_frequency_text),
	sma1305_clk_frequency_text);

static int sma1305_clk_frequency_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_0F_VBAT_TEMP_SENSING, &val);
	ucontrol->value.integer.value[0] = val & 0x03;

	return 0;
}

static int sma1305_clk_frequency_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_0F_VBAT_TEMP_SENSING, 0x03, sel);

	return 0;
}

/* 0x10[4:2] SPKMODE */
static const char * const sma1305_spkmode_text[] = {
	"Off", "Mono", "Reserved", "Reserved",
	"Stereo", "Reserved", "Reserved", "Reserved"};

static const struct soc_enum sma1305_spkmode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_spkmode_text),
	sma1305_spkmode_text);

static int sma1305_spkmode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_10_SYSTEM_CTRL1, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x1C) >> 2);

	return 0;
}

static int sma1305_spkmode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_10_SYSTEM_CTRL1, 0x1C, (sel << 2));

	if (sel == (SPK_MONO >> 2))
		sma1305->stereo_two_chip = false;
	else if (sel == (SPK_STEREO >> 2))
		sma1305->stereo_two_chip = true;

	return 0;
}

/* 0x12[7:6] INPUT */
static const char * const sma1305_input_gain_text[] = {
	"Gain_0dB", "Gain_M6dB", "Gain_M12dB", "Gain_MInf"};

static const struct soc_enum sma1305_input_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_input_gain_text),
		sma1305_input_gain_text);

static int sma1305_input_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_12_SYSTEM_CTRL3, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_input_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_12_SYSTEM_CTRL3, 0xC0, (sel << 6));

	return 0;
}

/* 0x12[5:4] INPUT_R */
static const char * const sma1305_input_r_gain_text[] = {
	"Gain_0dB", "Gain_M6dB", "Gain_M12dB", "Gain_MInf"};

static const struct soc_enum sma1305_input_r_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_input_r_gain_text),
		sma1305_input_r_gain_text);

static int sma1305_input_r_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_12_SYSTEM_CTRL3, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x30) >> 4);

	return 0;
}

static int sma1305_input_r_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_12_SYSTEM_CTRL3, 0x30, (sel << 4));

	return 0;
}

/* 0x13[3:0] SET_DLY */
static const char * const sma1305_set_dly_text[] = {
	"D_20usec", "D_40usec", "D_60usec", "D_80usec",
	"D_100usec", "D_120usec", "D_140usec", "D_160usec",
	"D_180usec", "D_200usec", "D_220usec", "D_240usec",
	"D_260usec", "D_280usec", "D_300usec", "D_320usec"};

static const struct soc_enum sma1305_set_dly_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_dly_text),
		sma1305_set_dly_text);

static int sma1305_set_dly_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_13_DELAY, &val);
	ucontrol->value.integer.value[0] = val & 0x0F;

	return 0;
}

static int sma1305_set_dly_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_13_DELAY, 0x0F, sel);

	return 0;
}

/* 0x14[7:6] SPK_HYSFB */
static const char * const sma1305_spk_hysfb_text[] = {
	"f_625kHz", "f_414kHz", "f_297kHz", "f_226kHz"};

static const struct soc_enum sma1305_spk_hysfb_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_spk_hysfb_text), sma1305_spk_hysfb_text);

static int sma1305_spk_hysfb_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_14_MODULATOR, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_spk_hysfb_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_14_MODULATOR, 0xC0, (sel << 6));

	return 0;
}

/* 0x14 Modulator */
static int spk_bdelay_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_14_MODULATOR);
}

static int spk_bdelay_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_14_MODULATOR);
}

/* 0x15~0x1B bass boost speaker coeff */
static int bass_spk_coeff_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_15_BASS_SPK1);
}

static int bass_spk_coeff_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_15_BASS_SPK1);
}

/* 0x1C[3:0] BOP_HOLD_TIME */
static const char * const sma1305_bop_hold_time_text[] = {
	"skip", "t_10usec", "t_20usec", "t_40usec",
	"t_80usec", "t_160usec", "t_320usec", "t_640usec",
	"t_1.28msec", "t_2.56msec", "t_5.12msec", "t_10.24msec",
	"t_20.48msec", "t_40.96msec", "t_81.92msec", "t_163.84msec"};

static const struct soc_enum sma1305_bop_hold_time_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_bop_hold_time_text),
		sma1305_bop_hold_time_text);

static int sma1305_bop_hold_time_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_1C_BROWN_OUT_PROT20, &val);
	ucontrol->value.integer.value[0] = val & 0x0F;

	return 0;
}

static int sma1305_bop_hold_time_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_1C_BROWN_OUT_PROT20, 0x0F, sel);

	return 0;
}

/* 0x1D Brown Out hysteresis */
static int bo_hys_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_1D_BROWN_OUT_PROT0);
}

static int bo_hys_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_1D_BROWN_OUT_PROT0);
}

/* 0x1E[4:1] TONE_FREQ */
static const char * const sma1305_tone_freq_text[] = {
	"f_50Hz", "f_60Hz", "f_140Hz", "f_150Hz",
	"f_175Hz", "f_180Hz", "f_200Hz", "f_375Hz",
	"f_750Hz", "f_1kHz", "f_3kHz", "f_6kHz",
	"f_11.75kHz", "f_15kHz", "f_17kHz", "f_19kHz"};

static const struct soc_enum sma1305_tone_freq_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_tone_freq_text), sma1305_tone_freq_text);

static int sma1305_tone_freq_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_1E_TONE_GENERATOR, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x1E) >> 1);

	return 0;
}

static int sma1305_tone_freq_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_1E_TONE_GENERATOR, 0x1E, (sel<<1));

	return 0;
}

/* 0x1F TONE_FINE VOLUME Set */
static int tone_fine_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_1F_TONE_FINE_VOLUME);
}

static int tone_fine_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_1F_TONE_FINE_VOLUME);
}

/* 0x22~0x26 Compressor/Limiter */
static int comp_hys_sel_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_22_COMP_HYS_SEL);
}

static int comp_hys_sel_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_22_COMP_HYS_SEL);
}

/* 0x27~0x32 BOP */
static int brown_out_prot_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_27_BROWN_OUT_PROT4);
}

static int brown_out_prot_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_27_BROWN_OUT_PROT4);
}

/* 0x34[3:2] OCP_FILTER */
static const char * const sma1305_ocp_filter_text[] = {
	"Filter_0(Slowest)", "Filter_1", "Filter_2", "Filter_3(Fastest)"};

static const struct soc_enum sma1305_ocp_filter_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_ocp_filter_text),
		sma1305_ocp_filter_text);

static int sma1305_ocp_filter_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_34_OCP_SPK, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x0C) >> 2);

	return 0;
}

static int sma1305_ocp_filter_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	if ((sel < 0) || (sel > 3))
		return -EINVAL;

	regmap_update_bits(sma1305->regmap, SMA1305_34_OCP_SPK,
			0x0C, (sel << 2));

	return 0;
}

/*0x34[1:0] OCP_LVL_N */
static const char * const sma1305_ocp_lvl_text[] = {
	"I_1.7A", "I_2.0A", "I_2.4A", "I_2.7A"};

static const struct soc_enum sma1305_ocp_lvl_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_ocp_lvl_text), sma1305_ocp_lvl_text);

static int sma1305_ocp_lvl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_34_OCP_SPK, &val);
	ucontrol->value.integer.value[0] = (val & 0x03);

	return 0;
}

static int sma1305_ocp_lvl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	if ((sel < 0) || (sel > 3))
		return -EINVAL;

	regmap_update_bits(sma1305->regmap, SMA1305_34_OCP_SPK, 0x03, sel);

	return 0;
}

/*0x35[7:6] I_OP1 */
static const char * const sma1305_i_op1_text[] = {
	"I_40uA(L20uA)", "I_80uA(L40uA)", "I_120uA(L60uA)", "I_160uA(L80uA)"};

static const struct soc_enum sma1305_i_op1_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_i_op1_text), sma1305_i_op1_text);

static int sma1305_i_op1_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_35_FDPEC_CTRL0, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_i_op1_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
			SMA1305_35_FDPEC_CTRL0,	0xC0, (sel<<6));

	return 0;
}

/*0x35[5:4] I_OP2 */
static const char * const sma1305_i_op2_text[] = {
	"I_30uA", "I_40uA", "I_50uA", "I_60uA"};

static const struct soc_enum sma1305_i_op2_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_i_op2_text), sma1305_i_op2_text);

static int sma1305_i_op2_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_35_FDPEC_CTRL0, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x30) >> 4);

	return 0;
}

static int sma1305_i_op2_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
			SMA1305_35_FDPEC_CTRL0, 0x30, (sel<<4));

	return 0;
}

/*0x35[1:0] FDPEC_GAIN */
static const char * const sma1305_fdpec_gain_text[] = {
	"RCV_x0.5", "RCV_x1.1", "SPK_x3.0", "SPK_x3.6"};

static const struct soc_enum sma1305_fdpec_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_fdpec_gain_text),
		sma1305_fdpec_gain_text);

static int sma1305_fdpec_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_35_FDPEC_CTRL0, &val);
	ucontrol->value.integer.value[0] = val & 0x03;

	return 0;
}

static int sma1305_fdpec_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap, SMA1305_35_FDPEC_CTRL0, 0x03, sel);

	return 0;
}

/* 0x36[6:5] LR_DELAY */
static const char * const sma1305_lr_delay_text[] = {
	"Delay_00", "Delay_01", "Delay_10", "Delay_11"};

static const struct soc_enum sma1305_lr_delay_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_lr_delay_text), sma1305_lr_delay_text);

static int sma1305_lr_delay_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_36_PROTECTION, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x60) >> 5);

	return 0;
}

static int sma1305_lr_delay_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_36_PROTECTION, 0x60, (sel << 5));

	return 0;
}

/* 0x36[1:0] OTP_MODE */
static const char * const sma1305_otp_mode_text[] = {
	"Disable", "I_L1_S_L2", "R_L1_S_L2", "S_L1_S_L2"};

static const struct soc_enum sma1305_otp_mode_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_otp_mode_text), sma1305_otp_mode_text);

static int sma1305_otp_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_36_PROTECTION, &val);
	ucontrol->value.integer.value[0] = (val & 0x03);

	return 0;
}

static int sma1305_otp_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap, SMA1305_36_PROTECTION, 0x03, sel);

	return 0;
}

/* Slope CTRL */
static int slope_ctrl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_37_SLOPECTRL);
}

static int slope_ctrl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_37_SLOPECTRL);
}

/* 0x38[5:4] PMT_UPDATE */
static const char * const sma1305_pmt_update_text[] = {
	"Time_40msec", "Time_80msec", "Time_160msec", "Time_320msec"};

static const struct soc_enum sma1305_pmt_update_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_pmt_update_text),
		sma1305_pmt_update_text);

static int sma1305_pmt_update_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_38_POWER_METER, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x30) >> 4);

	return 0;
}

static int sma1305_pmt_update_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
			SMA1305_38_POWER_METER, 0x30, (sel << 4));

	return 0;
}

/* 0x38, 0x39 Powermeter */
static int power_meter_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_38_POWER_METER);
}

static int power_meter_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_38_POWER_METER);
}

/* 0x3B ~ 0x3F Test 1~3, DIG_IDLE_CURRENT_CTRL, ATEST2 */
static int test_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_3B_TEST1);
}

static int test_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_3B_TEST1);
}

/* 0x8B ~ 0x8E PLL Setting */
static int pll_setting_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_8B_PLL_POST_N);
}

static int pll_setting_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_8B_PLL_POST_N);
}

/* 0x8F ANALOG_TEST */
static const char * const sma1305_gm_ctrl_text[] = {
	"S_10uA/V", "Reserved", "S_20uA/V", "S_30uA/V"};

static const struct soc_enum sma1305_gm_ctrl_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_gm_ctrl_text),
		sma1305_gm_ctrl_text);

static int sma1305_gm_ctrl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_8F_ANALOG_TEST, &val);
	ucontrol->value.integer.value[0] = (val & 0x03);

	return 0;
}

static int sma1305_gm_ctrl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
			SMA1305_8F_ANALOG_TEST, 0x03, sel);

	return 0;
}

/* 0x90, 0x91 CrestLim */
static int crest_lim_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_90_CRESTLIM1);
}

static int crest_lim_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_90_CRESTLIM1);
}

/* 0x92[7:4] FLT_VDD_GAIN */
static const char * const sma1305_flt_vdd_gain_text[] = {
	"VDD_2.6V", "VDD_2.65V", "VDD_2.7V", "VDD_2.75V",
	"VDD_2.8V", "VDD_2.85V", "VDD_2.9V", "VDD_2.95V",
	"VDD_3.0V", "VDD_3.05V", "VDD_3.1V", "VDD_3.15V",
	"VDD_3.2V", "VDD_3.25V", "VDD_3.3V", "VDD_3.35V"};

static const struct soc_enum sma1305_flt_vdd_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_flt_vdd_gain_text),
		sma1305_flt_vdd_gain_text);

static int sma1305_flt_vdd_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_92_FDPEC_CTRL1, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xF0) >> 4);

	return 0;
}

static int sma1305_flt_vdd_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	sma1305->flt_vdd_gain_status = sel;

	regmap_update_bits(sma1305->regmap,
		SMA1305_92_FDPEC_CTRL1, 0xF0, (sel << 4));

	return 0;
}

/* 0x94[7:6] SLOPE_OFF */
static const char * const sma1305_slope_off_text[] = {
"Time_6.7ns", "Time_4.8ns", "Time_2.6ns", "Time_1.2ns"};

static const struct soc_enum sma1305_slope_off_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_slope_off_text), sma1305_slope_off_text);

static int sma1305_slope_off_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_94_BOOST_CTRL9, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_slope_off_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_94_BOOST_CTRL9, 0xC0, (sel << 6));

	return 0;
}

/* 0x94[5:4] SLOPE */
static const char * const sma1305_slope_text[] = {
"Time_6.7ns", "Time_4.8ns", "Time_2.6ns", "Time_1.2ns"};

static const struct soc_enum sma1305_slope_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_slope_text), sma1305_slope_text);

static int sma1305_slope_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_94_BOOST_CTRL9, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x30) >> 4);

	return 0;
}

static int sma1305_slope_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_94_BOOST_CTRL9, 0x30, (sel << 4));

	return 0;
}

/* 0x94[2:0] SET_RMP */
static const char * const sma1305_set_rmp_text[] = {
	"RMP_3.0A/us", "RMP_4.0A/us", "RMP_5.0A/us", "RMP_6.0A/us",
	"RMP_7.0A/us", "RMP_8.0A/us", "RMP_9.0A/us", "RMP_10.0A/us"};

static const struct soc_enum sma1305_set_rmp_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_rmp_text), sma1305_set_rmp_text);

static int sma1305_set_rmp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_94_BOOST_CTRL9, &val);
	ucontrol->value.integer.value[0] = val & 0x07;

	return 0;
}

static int sma1305_set_rmp_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_94_BOOST_CTRL9, 0x07, sel);

	return 0;
}

/* 0x95[6:4] SET_OCL */
static const char * const sma1305_set_ocl_text[] = {
	"I_2.6A(L1.7A)", "I_2.8A(L1.8A)", "I_3.0A(L2.0A)", "I_3.3A(L2.2A)",
	"I_3.6A(L2.5A)", "I_4.0A(L2.7A)", "I_4.5A(L3.1A)", "I_5.1A(L3.5A)"};

static const struct soc_enum sma1305_set_ocl_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_ocl_text), sma1305_set_ocl_text);

static int sma1305_set_ocl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_95_BOOST_CTRL10, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x70) >> 4);

	return 0;
}

static int sma1305_set_ocl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_95_BOOST_CTRL10, 0x70, (sel << 4));

	return 0;
}

/* 0x95[3:2] SET_COMP I-gain */
static const char * const sma1305_set_comp_i_text[] = {
	"IG_30pF", "IG_60pF", "IG_90pF", "IG_120pF"};

static const struct soc_enum sma1305_set_comp_i_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_comp_i_text),
			sma1305_set_comp_i_text);

static int sma1305_set_comp_i_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_95_BOOST_CTRL10, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x0C) >> 2);

	return 0;
}

static int sma1305_set_comp_i_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_95_BOOST_CTRL10, 0x0C, (sel << 2));

	return 0;
}

/* 0x95[1:0] SET_COMP P-gain */
static const char * const sma1305_set_comp_p_text[] = {
	"PG_0.5Mohm", "PG_0.375Mohm", "PG_0.25Mohm", "PG_0.125Mohm"};

static const struct soc_enum sma1305_set_comp_p_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_comp_p_text),
			sma1305_set_comp_p_text);

static int sma1305_set_comp_p_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_95_BOOST_CTRL10, &val);
	ucontrol->value.integer.value[0] = val & 0x03;

	return 0;
}

static int sma1305_set_comp_p_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_95_BOOST_CTRL10, 0x03, sel);

	return 0;
}

/* 0x96[7:4] SET_DT */
static const char * const sma1305_set_dt_text[] = {
	"Time_16.0ns", "Time_14.0ns", "Time_12.0ns", "Time_11.0ns",
	"Time_10.0ns", "Time_9.0ns", "Time_8.0ns", "Time_7.3ns",
	"Time_6.7ns", "Time_6.2ns", "Time_5.8ns", "Time_5.4ns",
	"Time_5.1ns", "Time_4.8ns", "Time_2.1ns", "Time_2.1ns"};

static const struct soc_enum sma1305_set_dt_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_dt_text), sma1305_set_dt_text);

static int sma1305_set_dt_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_96_BOOST_CTRL11, &val);
	ucontrol->value.integer.value[0] = ((val & 0xF0) >> 4);

	return 0;
}

static int sma1305_set_dt_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_96_BOOST_CTRL11, 0xF0, (sel << 4));

	return 0;
}

/* 0x96[3:0] SET_DT_OFF */
static const char * const sma1305_set_dt_off_text[] = {
	"Time_16.0ns", "Time_14.0ns", "Time_12.0ns", "Time_11.0ns",
	"Time_10.0ns", "Time_9.0ns", "Time_8.0ns", "Time_7.3ns",
	"Time_6.7ns", "Time_6.2ns", "Time_5.8ns", "Time_5.4ns",
	"Time_5.1ns", "Time_4.8ns", "Time_2.1ns", "Time_2.1ns"};

static const struct soc_enum sma1305_set_dt_off_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_set_dt_off_text),
		sma1305_set_dt_off_text);

static int sma1305_set_dt_off_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_96_BOOST_CTRL11, &val);
	ucontrol->value.integer.value[0] = val & 0x0F;

	return 0;
}

static int sma1305_set_dt_off_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_96_BOOST_CTRL11, 0x0F, sel);

	return 0;
}

/* 0x97~0x9A OTP Trimming 0~# Set */
static int otp_trimming_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_97_OTP_TRM0);
}

static int otp_trimming_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_97_OTP_TRM0);
}

/* 0xA2[5:4] PLL_DIV */
static const char * const sma1305_pll_div_text[] = {
	"PLL_OUT", "PLL_OUT/2", "PLL_OUT/4", "PLL_OUT/8"};

static const struct soc_enum sma1305_pll_div_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_pll_div_text), sma1305_pll_div_text);

static int sma1305_pll_div_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A2_TOP_MAN1, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x30) >> 4);

	return 0;
}

static int sma1305_pll_div_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A2_TOP_MAN1, 0x30, (sel << 4));

	return 0;
}

/* 0xA3[7:6] MON_SOC_PLL */
static const char * const sma1305_mon_osc_pll_text[] = {
	"Data output at SDO", "Monitoring PLL at SDO", "Reserved",
	"Monitoring internal OSC at SDO"};

static const struct soc_enum sma1305_mon_osc_pll_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_mon_osc_pll_text),
		sma1305_mon_osc_pll_text);

static int sma1305_mon_osc_pll_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A3_TOP_MAN2, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_mon_osc_pll_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A3_TOP_MAN2, 0xC0, (sel << 6));

	return 0;
}


/* 0xA4[7:5] INTERFACE */
static const char * const sma1305_interface_text[] = {
	"Reserved", "LJ", "I2S", "Reserved",
	"TDM", "Reserved", "Reserved", "Reserved"};

static const struct soc_enum sma1305_interface_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_interface_text), sma1305_interface_text);

static int sma1305_interface_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A4_TOP_MAN3, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xE0) >> 5);

	return 0;
}

static int sma1305_interface_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A4_TOP_MAN3, 0xE0, (sel << 5));

	return 0;
}

/* 0xA4[4:3] SCK_RATE */
static const char * const sma1305_sck_rate_text[] = {
	"fs_64", "fs_64", "fs_32", "fs_32"};

static const struct soc_enum sma1305_sck_rate_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_sck_rate_text), sma1305_sck_rate_text);

static int sma1305_sck_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A4_TOP_MAN3, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x18) >> 3);

	return 0;
}

static int sma1305_sck_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A4_TOP_MAN3, 0x18, (sel << 3));

	return 0;
}

/* 0xA4[2:1] DATA_W */
static const char * const sma1305_data_w_text[] = {
	"width_24bit", "Reserved", "Reserved", "width_16bit"};

static const struct soc_enum sma1305_data_w_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_data_w_text), sma1305_data_w_text);

static int sma1305_data_w_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A4_TOP_MAN3, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x06) >> 1);

	return 0;
}

static int sma1305_data_w_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A4_TOP_MAN3, 0x06, (sel << 1));

	return 0;
}

/* 0xA5[5:3] TDM_SLOT1_POS_RX */
static const char * const sma1305_tdm_slot1_rx_text[] = {
	"0", "1", "2", "3", "4", "5", "6", "7"};

static const struct soc_enum sma1305_tdm_slot1_rx_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_tdm_slot1_rx_text),
		sma1305_tdm_slot1_rx_text);

static int sma1305_tdm_slot1_rx_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A5_TDM1, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x38) >> 3);

	return 0;
}

static int sma1305_tdm_slot1_rx_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A5_TDM1, 0x38, (sel << 3));

	return 0;
}

/* 0xA5[2:0] TDM_SLOT2_POS_RX */
static const char * const sma1305_tdm_slot2_rx_text[] = {
	"0", "1", "2", "3", "4", "5", "6", "7"};

static const struct soc_enum sma1305_tdm_slot2_rx_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_tdm_slot2_rx_text),
		sma1305_tdm_slot2_rx_text);

static int sma1305_tdm_slot2_rx_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A5_TDM1, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x07) >> 0);

	return 0;
}

static int sma1305_tdm_slot2_rx_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A5_TDM1, 0x07, (sel << 0));

	return 0;
}

/* 0xA6[5:3] TDM_SLOT1_POS_TX */
static const char * const sma1305_tdm_slot1_tx_text[] = {
	"0", "1", "2", "3", "4", "5", "6", "7"};

static const struct soc_enum sma1305_tdm_slot1_tx_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_tdm_slot1_tx_text),
			sma1305_tdm_slot1_tx_text);

static int sma1305_tdm_slot1_tx_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A6_TDM2, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x38) >> 3);

	return 0;
}

static int sma1305_tdm_slot1_tx_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A6_TDM2, 0x38, (sel << 3));

	return 0;
}

/* 0xA6[2:0] TDM_SLOT2_POS_TX */
static const char * const sma1305_tdm_slot2_tx_text[] = {
	"0", "1", "2", "3", "4", "5", "6", "7"};

static const struct soc_enum sma1305_tdm_slot2_tx_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_tdm_slot2_tx_text),
			sma1305_tdm_slot2_tx_text);

static int sma1305_tdm_slot2_tx_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A6_TDM2, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x07) >> 0);

	return 0;
}

static int sma1305_tdm_slot2_tx_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A6_TDM2, 0x07, (sel << 0));

	return 0;
}

/* 0xA7[7:6] CLK_MON_TIME_SEL */
static const char * const sma1305_clk_mon_time_sel_text[] = {
	"Time_80us", "Time_40us", "Time_20us", "Time_10us"};

static const struct soc_enum sma1305_clk_mon_time_sel_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_clk_mon_time_sel_text),
			sma1305_clk_mon_time_sel_text);

static int sma1305_clk_mon_time_sel_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_A7_CLK_MON, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0xC0) >> 6);

	return 0;
}

static int sma1305_clk_mon_time_sel_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_A7_CLK_MON, 0xC0, (sel << 6));

	return 0;
}

/* 0xAC[6:5] BOOST_MODE */
static const char * const sma1305_boost_mode_text[] = {
	"Class_H", "Forced_On", "Off", "Reserved"};

static const struct soc_enum sma1305_boost_mode_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_boost_mode_text),
		sma1305_boost_mode_text);

static int sma1305_boost_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_AC_BOOST_CTRL5, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x60) >> 5);

	return 0;
}

static int sma1305_boost_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_AC_BOOST_CTRL5, 0x60, (sel << 5));

	return 0;
}

/* 0xAE[4:2] BST_FREQ */
static const char * const sma1305_bst_freq_text[] = {
	"f_1.4MHz", "f_1.5MHz", "f_1.8MHz", "f_2.0MHz",
	"f_2.2MHz", "f_2.5MHz", "f_2.7MHz", "f_3.1MHz"};

static const struct soc_enum sma1305_bst_freq_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_bst_freq_text), sma1305_bst_freq_text);

static int sma1305_bst_freq_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(sma1305->regmap, SMA1305_AE_BOOST_CTRL7, &val);
	ucontrol->value.integer.value[0] = (long) (((long) val & 0x1C) >> 2);

	return 0;
}

static int sma1305_bst_freq_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_AE_BOOST_CTRL7, 0x1C, (sel << 2));

	return 0;
}

/* 0xAE[1:0] MIN_DUTY */
static const char * const sma1305_min_duty_text[] = {
	"Time_40ns", "Time_80ns", "Time_120ns", "Time_160ns"};

static const struct soc_enum sma1305_min_duty_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1305_min_duty_text), sma1305_min_duty_text);

static int sma1305_min_duty_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int val;

	regmap_read(sma1305->regmap, SMA1305_AE_BOOST_CTRL7, &val);
	ucontrol->value.integer.value[0] = val & 0x03;

	return 0;
}

static int sma1305_min_duty_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1305->regmap,
		SMA1305_AE_BOOST_CTRL7, 0x03, sel);

	return 0;
}

/* 0xA8~0xAE BOOST_CONTROL */
static int boost_ctrl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_A8_BOOST_CTRL1);
}

static int boost_ctrl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_A8_BOOST_CTRL1);
}

/* 0xAF LPF */
static int lpf_ctrl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_AF_LPF);
}

static int lpf_ctrl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_AF_LPF);
}

/* 0xB0~0xB5 POWER METER2 */
static int power_meter2_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1305_B0_RMS_TC1);
}

static int power_meter2_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1305_B0_RMS_TC1);
}

static const char * const speaker_receiver_mode_text[] = {
	"Speaker", "Receiver(0.1W)", "Receiver(0.5W)"};

static const struct soc_enum speaker_receiver_mode_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_receiver_mode_text),
	speaker_receiver_mode_text);

static int speaker_receiver_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = sma1305->spk_rcv_mode;

	return 0;
}

static int speaker_receiver_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	sma1305->spk_rcv_mode = (unsigned int) ucontrol->value.integer.value[0];

	sma1305_spk_rcv_conf(component);

	return 0;
}

static const struct snd_kcontrol_new sma1305_snd_controls[] = {
/* System CTRL [0x00] */
SOC_SINGLE("I2C Reg Reset(1:reset_0:normal)",
		SMA1305_00_SYSTEM_CTRL, 1, 1, 0),
SOC_SINGLE_EXT("Power Up(1:Up_0:Down)", SND_SOC_NOPM, 0, 1, 0,
	power_up_down_control_get, power_up_down_control_put),
SOC_SINGLE_EXT("Force AMP Power Down", SND_SOC_NOPM, 0, 1, 0,
	power_down_control_get, power_down_control_put),

SOC_SINGLE_EXT("Force SDO Bypass", SND_SOC_NOPM, 0, 1, 0,
	force_sdo_bypass_get, force_sdo_bypass_put),

/* Input CTRL1 [0x01] */
SOC_ENUM_EXT("I2S input format(I2S_LJ_RJ)", sma1305_input_format_enum,
	sma1305_input_format_get, sma1305_input_format_put),
SOC_SINGLE("First-channel pol I2S(1:H_0:L)",
		SMA1305_01_INPUT_CTRL1, 3, 1, 0),
SOC_SINGLE("Data written SCK edge(1:R_0:F)",
		SMA1305_01_INPUT_CTRL1, 2, 1, 0),

/* Brown Out Protection [0x02 ~ 0x08] */
SND_SOC_BYTES_EXT("Brown Out Level Setting", 7,
	bo_lvl_get, bo_lvl_put),

/* Output CTRL [0x09] */
SOC_ENUM_EXT("Port In/Out port config", sma1305_port_config_enum,
	sma1305_port_config_get, sma1305_port_config_put),
SOC_ENUM_EXT("SDO Output1 Source", sma1305_sdo_out1_sel_enum,
	sma1305_sdo_out1_sel_get, sma1305_sdo_out1_sel_put),
SOC_ENUM_EXT("SDO Output0 Source", sma1305_sdo_out0_sel_enum,
	sma1305_sdo_out0_sel_get, sma1305_sdo_out0_sel_put),

/* SPK_VOL [0x0A] */
SOC_SINGLE_TLV("Speaker Volume", SMA1305_0A_SPK_VOL,
		0, 167, 1, sma1305_spk_tlv),

/* BOOST_CTRL8 [0x0C] */
SOC_ENUM_EXT("High Side OCP lvl", sma1305_set_ocp_h_enum,
	sma1305_set_ocp_h_get, sma1305_set_ocp_h_put),
SOC_SINGLE("Low OCL mode(1:Noraml_0:Low_OCL)",
		SMA1305_0C_BOOST_CTRL8, 3, 1, 0),

/* MUTE_VOL_CTRL [0x0E] */
SOC_ENUM_EXT("Volume slope", sma1305_vol_slope_enum,
	sma1305_vol_slope_get, sma1305_vol_slope_put),
SOC_ENUM_EXT("Mute slope", sma1305_mute_slope_enum,
	sma1305_mute_slope_get, sma1305_mute_slope_put),
SOC_SINGLE("Speaker Mute Switch(1:muted_0:un)",
		SMA1305_0E_MUTE_VOL_CTRL, 0, 1, 0),

/* VBAT_TEMP_SENSING [0x0F] */
SOC_SINGLE("VBAT & Temp Sensing(1:Off_0:On)",
		SMA1305_0F_VBAT_TEMP_SENSING, 7, 1, 0),
SOC_ENUM_EXT("VBAT LPF Bypass", sma1305_vbat_lpf_byp_enum,
	sma1305_vbat_lpf_byp_get, sma1305_vbat_lpf_byp_put),
SOC_ENUM_EXT("VBAT & Temp Sampling Freq", sma1305_clk_frequency_enum,
	sma1305_clk_frequency_get, sma1305_clk_frequency_put),

/* System CTRL1 [0x10] */
SOC_ENUM_EXT("Speaker Mode", sma1305_spkmode_enum,
	sma1305_spkmode_get, sma1305_spkmode_put),

/* System CTRL2 [0x11] */
SOC_SINGLE("Speaker Bass(1:en_0:dis)", SMA1305_11_SYSTEM_CTRL2, 6, 1, 0),
SOC_SINGLE("Speaker Comp/Limiter(1:en_0:dis)",
		SMA1305_11_SYSTEM_CTRL2, 5, 1, 0),
SOC_SINGLE("LR_DATA_SW(1:swap_0:normal)", SMA1305_11_SYSTEM_CTRL2, 4, 1, 0),
SOC_SINGLE("Mono Mix(1:en_0:dis)", SMA1305_11_SYSTEM_CTRL2, 0, 1, 0),

/* System CTRL3 [0x12] */
SOC_ENUM_EXT("Input gain", sma1305_input_gain_enum,
	sma1305_input_gain_get, sma1305_input_gain_put),
SOC_ENUM_EXT("Input gain for right channel", sma1305_input_r_gain_enum,
	sma1305_input_r_gain_get, sma1305_input_r_gain_put),

/* Delay [0x13] */
SOC_SINGLE("Delay(1:Bypass_0:On)", SMA1305_13_DELAY, 4, 1, 0),
SOC_ENUM_EXT("Delay Setting", sma1305_set_dly_enum,
	sma1305_set_dly_get, sma1305_set_dly_put),

/* Modulator [0x14] */
SOC_ENUM_EXT("Speaker HYSFB", sma1305_spk_hysfb_enum,
	sma1305_spk_hysfb_get, sma1305_spk_hysfb_put),
SND_SOC_BYTES_EXT("Speaker BDELAY", 1, spk_bdelay_get, spk_bdelay_put),

/* BassSpk1~7 [0x15 ~ 0x1B] */
SND_SOC_BYTES_EXT("Bass Boost SPK Coeff", 7,
	bass_spk_coeff_get, bass_spk_coeff_put),

/* BROWN_OUT_PROT20 [0x1C] */
SOC_ENUM_EXT("BOP Hold Time", sma1305_bop_hold_time_enum,
	sma1305_bop_hold_time_get, sma1305_bop_hold_time_put),

/* BROWN_OUT_PROT0 [0x1D] */
SOC_SINGLE("BrownOut Protection(1:On_0:Off)",
	SMA1305_1D_BROWN_OUT_PROT0, 7, 1, 0),
SND_SOC_BYTES_EXT("BrownOut Hysteresis", 1,
	bo_hys_get, bo_hys_put),

/* TONE GENERATOR [0x1E] */
SOC_SINGLE("Tone Fine Volume(1:Bypass_0:On)",
	SMA1305_1E_TONE_GENERATOR, 6, 1, 0),
SOC_SINGLE("Tone Audio Mixing(1:En_0:Dis)",
	SMA1305_1E_TONE_GENERATOR, 5, 1, 0),
SOC_ENUM_EXT("Tone Frequency", sma1305_tone_freq_enum,
	sma1305_tone_freq_get, sma1305_tone_freq_put),
SOC_SINGLE("Tone Generator(1:On_0:Off)",
	SMA1305_1E_TONE_GENERATOR, 0, 1, 0),

/* Tone FINE VOLUME [0x1F] */
SND_SOC_BYTES_EXT("Tone Fine Volume", 1,
	tone_fine_volume_get, tone_fine_volume_put),

/* Comp_Lim1~4 [0x22 ~ 0x26] */
SND_SOC_BYTES_EXT("Comp/Limiter Cotnrol", 5,
	comp_hys_sel_get, comp_hys_sel_put),

/* BROWN_OUT_PROT [0x27 ~ 0x32] */
SND_SOC_BYTES_EXT("Brown Out Protection", 12,
	brown_out_prot_get, brown_out_prot_put),

/* OCP_SPK [0x34] */
SOC_ENUM_EXT("OCP Filter Time", sma1305_ocp_filter_enum,
	sma1305_ocp_filter_get, sma1305_ocp_filter_put),
SOC_ENUM_EXT("OCP Level Time", sma1305_ocp_lvl_enum,
	sma1305_ocp_lvl_get, sma1305_ocp_lvl_put),

/* FDPEC_CTRL0 [0x35] */
SOC_ENUM_EXT("OP1 Current Control", sma1305_i_op1_enum,
	sma1305_i_op1_get, sma1305_i_op1_put),
SOC_ENUM_EXT("OP2 Current Control", sma1305_i_op2_enum,
	sma1305_i_op2_get, sma1305_i_op2_put),
SOC_SINGLE("Sync Resistor(1:10kohm_0:64kohm)",
	SMA1305_35_FDPEC_CTRL0, 2, 1, 0),
SOC_ENUM_EXT("SPK_AMP Analog Loop Gain", sma1305_fdpec_gain_enum,
	sma1305_fdpec_gain_get, sma1305_fdpec_gain_put),

/* Protection [0x36] */
SOC_SINGLE("Edge displacement(1:dis_0:en)",
		SMA1305_36_PROTECTION, 7, 1, 0),
SOC_ENUM_EXT("PWM LR delay", sma1305_lr_delay_enum,
		sma1305_lr_delay_get, sma1305_lr_delay_put),
SOC_SINGLE("OCP spk output state(1:dis_0:en)",
		SMA1305_36_PROTECTION, 3, 1, 0),
SOC_SINGLE("OCP mode(1:SD_0:AR)",
		SMA1305_36_PROTECTION, 2, 1, 0),
SOC_ENUM_EXT("OTP mode", sma1305_otp_mode_enum,
		sma1305_otp_mode_get, sma1305_otp_mode_put),

/* Slope CTRL [0x37] */
SND_SOC_BYTES_EXT("SlopeCTRL", 1, slope_ctrl_get, slope_ctrl_put),

/* Power Meter [0x38 ~ 0x39] */
SOC_SINGLE("Power Meter(1:En_0:Dis)",
		SMA1305_38_POWER_METER, 7, 1, 0),
SOC_SINGLE("PM Initialization(1:En_0:sig_in)",
		SMA1305_38_POWER_METER, 6, 1, 0),
SOC_ENUM_EXT("Limiter Level Update Time", sma1305_pmt_update_enum,
	sma1305_pmt_update_get, sma1305_pmt_update_put),
SND_SOC_BYTES_EXT("Power Meter Hysteresis", 2,
	power_meter_get, power_meter_put),


/* Test1~3, DIG_IDLE_CURRENT, ATEST2 [0x3B ~ 0x3F] */
SND_SOC_BYTES_EXT("Test mode(Test 1~3_ATEST 1~2)",
		5, test_mode_get, test_mode_put),

/* PLL Setting [0x8B ~ 0x8E] */
SND_SOC_BYTES_EXT("PLL Setting", 4, pll_setting_get, pll_setting_put),

/* Analog Test [0x8F] */
SOC_SINGLE("Low UVLO(1:2p37_0:2p50)",
		SMA1305_8F_ANALOG_TEST, 2, 1, 0),
SOC_ENUM_EXT("OTA GM Control", sma1305_gm_ctrl_enum,
	sma1305_gm_ctrl_get, sma1305_gm_ctrl_put),

/* CrestLim [0x90 ~ 0x91] */
SND_SOC_BYTES_EXT("Crest Limit", 2,
		crest_lim_get, crest_lim_put),

/* FDPEC CTRL1 [0x92] */
SOC_ENUM_EXT("Filtered VDD gain control", sma1305_flt_vdd_gain_enum,
	sma1305_flt_vdd_gain_get, sma1305_flt_vdd_gain_put),
SOC_SINGLE("Fast charge(1:Dis_0:En)",
		SMA1305_92_FDPEC_CTRL1, 2, 1, 0),

/* INT Control [0x93] */
SOC_SINGLE("INT Operation(1:Manual_0:Auto)",
		SMA1305_93_INT_CTRL, 2, 1, 0),
SOC_SINGLE("INT Clear(1:Clear_0:Ready)",
		SMA1305_93_INT_CTRL, 1, 1, 0),
SOC_SINGLE("INT pin(1:Skip_0:INTN)",
		SMA1305_93_INT_CTRL, 0, 1, 0),

/* Boost CTRL9 [0x94] */
SOC_ENUM_EXT("Set switching Off Slew", sma1305_slope_off_enum,
	sma1305_slope_off_get, sma1305_slope_off_put),
SOC_ENUM_EXT("Set switching Slew", sma1305_slope_enum,
	sma1305_slope_get, sma1305_slope_put),
SOC_ENUM_EXT("Set Ramp Compensation", sma1305_set_rmp_enum,
	sma1305_set_rmp_get, sma1305_set_rmp_put),

/* Boost CTRL10 [0x95] */
SOC_SINGLE("P gain Selection(1:Normal_0:High)",
		SMA1305_95_BOOST_CTRL10, 7, 1, 0),
SOC_ENUM_EXT("Set Over current Limit", sma1305_set_ocl_enum,
	sma1305_set_ocl_get, sma1305_set_ocl_put),
SOC_ENUM_EXT("Set loop comp I gain", sma1305_set_comp_i_enum,
	sma1305_set_comp_i_get, sma1305_set_comp_i_put),
SOC_ENUM_EXT("Set loop comp P gain", sma1305_set_comp_p_enum,
	sma1305_set_comp_p_get, sma1305_set_comp_p_put),

/* Boost CTRL11 [0x96] */
SOC_ENUM_EXT("Set Driver Deadtime", sma1305_set_dt_enum,
	sma1305_set_dt_get, sma1305_set_dt_put),
SOC_ENUM_EXT("Set Driver Off Deadtime", sma1305_set_dt_off_enum,
	sma1305_set_dt_off_get, sma1305_set_dt_off_put),

/* OTP_TRM [0x97 ~ 0x9A] */
SND_SOC_BYTES_EXT("OTP Trimming", 4,
	otp_trimming_get, otp_trimming_put),

/* TOP_MAN1 [0xA2] */
SOC_SINGLE("PLL Lock Skip Mode(1:Dis_0:En)",
		SMA1305_A2_TOP_MAN1, 7, 1, 0),
SOC_SINGLE("PLL Power(1:Off_0:On)",
		SMA1305_A2_TOP_MAN1, 6, 1, 0),
SOC_ENUM_EXT("PLL Test Divider at SDO", sma1305_pll_div_enum,
		sma1305_pll_div_get, sma1305_pll_div_put),
SOC_SINGLE("LRSYNC(1:Dis_0:En)",
		SMA1305_A2_TOP_MAN1, 2, 1, 0),
SOC_SINGLE("SDO Pad Output Ctrl(1:L_0:H)",
		SMA1305_A2_TOP_MAN1, 1, 1, 0),
SOC_SINGLE("SDO output2(1:Two_0:One)",
		SMA1305_A2_TOP_MAN1, 0, 1, 0),

/* TOP_MAN2 [0xA3] */
SOC_ENUM_EXT("Monitoring OSC and PLL", sma1305_mon_osc_pll_enum,
	sma1305_mon_osc_pll_get, sma1305_mon_osc_pll_put),
SOC_SINGLE("SDO output1(1:HighZ_0:Logic)",
		SMA1305_A3_TOP_MAN2, 3, 1, 0),
SOC_SINGLE("SDO output3(1:24k_0:48k)",
		SMA1305_A3_TOP_MAN2, 2, 1, 0),
SOC_SINGLE("Clk Monitor(1:Not_0:Mon)",
		SMA1305_A3_TOP_MAN2, 1, 1, 0),
SOC_SINGLE("Internal OSC(1:Off_0:On)",
		SMA1305_A3_TOP_MAN2, 0, 1, 0),

/* TOP_MAN3 [0xA4] */
SOC_ENUM_EXT("Interface Format", sma1305_interface_enum,
	sma1305_interface_get, sma1305_interface_put),
SOC_ENUM_EXT("I2S SCK rate", sma1305_sck_rate_enum,
	sma1305_sck_rate_get, sma1305_sck_rate_put),
SOC_ENUM_EXT("I2S Data width", sma1305_data_w_enum,
	sma1305_data_w_get, sma1305_data_w_put),
SOC_SINGLE("LRCK Pol(1:H_0:L)",	SMA1305_A4_TOP_MAN3, 0, 1, 0),

/* TDM1 [0xA5] */
SOC_SINGLE("TDM Tx(1:stereo_0:mono)",
		SMA1305_A5_TDM1, 6, 1, 0),
SOC_ENUM_EXT("TDM slot1 pos Rx", sma1305_tdm_slot1_rx_enum,
		sma1305_tdm_slot1_rx_get, sma1305_tdm_slot1_rx_put),
SOC_ENUM_EXT("TDM slot2 pos Rx", sma1305_tdm_slot2_rx_enum,
		sma1305_tdm_slot2_rx_get, sma1305_tdm_slot2_rx_put),

/* TDM2 [0xA6] */
SOC_SINGLE("TDM Data Length(1:32_0:16)",
		SMA1305_A6_TDM2, 7, 1, 0),
SOC_SINGLE("TDM n-slot(1:8_0:4)",
		SMA1305_A6_TDM2, 6, 1, 0),
SOC_ENUM_EXT("TDM slot1 pos Tx", sma1305_tdm_slot1_tx_enum,
		sma1305_tdm_slot1_tx_get, sma1305_tdm_slot1_tx_put),
SOC_ENUM_EXT("TDM slot2 pos Tx", sma1305_tdm_slot2_tx_enum,
		sma1305_tdm_slot2_tx_get, sma1305_tdm_slot2_tx_put),

/* CLK_MON [0xA7] */
SOC_ENUM_EXT("Clk monitor time select", sma1305_clk_mon_time_sel_enum,
		sma1305_clk_mon_time_sel_get, sma1305_clk_mon_time_sel_put),
SOC_SINGLE("Dither Gen mod(1:On_0:Off)",
		SMA1305_A7_CLK_MON, 5, 1, 0),
SOC_SINGLE("Additional Tone Volume(1:On_0:Off)",
		SMA1305_A7_CLK_MON, 4, 1, 0),
SOC_SINGLE("Limiter Oper Range(1:En_0:Dis)",
		SMA1305_A7_CLK_MON, 3, 1, 0),

/* Boost CTRL [0xA8 ~ 0xAE] */
SND_SOC_BYTES_EXT("Boost Control", 7,
		boost_ctrl_get, boost_ctrl_put),

/* LPF [0xAF] */
SND_SOC_BYTES_EXT("LPF Tuning Coef", 1,
		lpf_ctrl_get, lpf_ctrl_put),

/* Power Meter2 [0xB0 ~ 0xB5] */
SND_SOC_BYTES_EXT("Power Meter Control", 6,
		power_meter2_get, power_meter2_put),

/* Boost CTRL5 [0xAC] */
SOC_ENUM_EXT("Boost Mode Control", sma1305_boost_mode_enum,
		sma1305_boost_mode_get, sma1305_boost_mode_put),

/* Boost CTRL7 [0xAE] */
SOC_ENUM_EXT("Set Boost Frequency", sma1305_bst_freq_enum,
		sma1305_bst_freq_get, sma1305_bst_freq_put),
SOC_ENUM_EXT("Set Minimum on time", sma1305_min_duty_enum,
		sma1305_min_duty_get, sma1305_min_duty_put),

SOC_ENUM_EXT("Speaker/Receiver Mode",
	speaker_receiver_mode_enum,
	speaker_receiver_mode_get, speaker_receiver_mode_put),
};

static int sma1305_spk_rcv_conf(struct snd_soc_component *component)
{
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	if (sma1305->spk_rcv_mode > 2)
		return -EINVAL;

	dev_info(component->dev, "%s : [%s] Mode\n", __func__,
			speaker_receiver_mode_text[sma1305->spk_rcv_mode]);

	switch (sma1305->spk_rcv_mode) {
	case SMA1305_RECEIVER_0P1W_MODE:
		/* SPK Volume : -1.0dB */
		regmap_write(sma1305->regmap, SMA1305_0A_SPK_VOL, 0x32);
		/* Shoot Through Protection : Enable */
		regmap_write(sma1305->regmap, SMA1305_0B_BST_TEST, 0xD0);
		/* VBAT & Temperature Sensing Off, LPF Bypass */
		regmap_write(sma1305->regmap,
				SMA1305_0F_VBAT_TEMP_SENSING, 0xE8);
		/* Delay Off */
		regmap_write(sma1305->regmap, SMA1305_13_DELAY, 0x19);
		/* HYSFB : 414kHz, BDELAY : 6'b011100 */
		regmap_write(sma1305->regmap, SMA1305_14_MODULATOR, 0x5C);
		/* Tone Generator(Volume - Off) & Fine volume Bypass */
		regmap_write(sma1305->regmap, SMA1305_1E_TONE_GENERATOR, 0xE1);
		/* Limiter Attack Level : 0.3ms, Release Time : 0.1s */
		regmap_write(sma1305->regmap, SMA1305_24_COMPLIM2, 0x04);
		/* OP1 : 40uA(LOW_PWR), OP2 : 30uA, High R(64kohm), RCVx0.5 */
		regmap_write(sma1305->regmap, SMA1305_35_FDPEC_CTRL0, 0x40);
		/* ENV_TRA, BOP_CTRL Enable */
		regmap_write(sma1305->regmap, SMA1305_3E_IDLE_MODE_CTRL, 0x07);
		/* OTA GM : 10uA/V */
		regmap_write(sma1305->regmap, SMA1305_8F_ANALOG_TEST, 0x00);
		/* FLT_VDD_GAIN : 3.15V */
		regmap_write(sma1305->regmap, SMA1305_92_FDPEC_CTRL1, 0xB0);
		/* Switching Off Slew : 2.6ns, Switching Slew : 4.8ns,
		 * Ramp Compensation : 4.0A/us
		 */
		regmap_write(sma1305->regmap, SMA1305_94_BOOST_CTRL9, 0x91);
		/* High P-gain, OCL : 5.1A */
		regmap_write(sma1305->regmap, SMA1305_95_BOOST_CTRL10, 0x74);
		/* Driver On Deadtime : 2.1ns, Driver Off Deadtime : 2.1ns */
		regmap_write(sma1305->regmap, SMA1305_96_BOOST_CTRL11, 0xFF);
		/* Min V : 5'b00101 (0.53V) */
		regmap_write(sma1305->regmap, SMA1305_A8_BOOST_CTRL1, 0x05);
		/* HEAD_ROOM : 5'b00111 (0.747V) */
		regmap_write(sma1305->regmap, SMA1305_A9_BOOST_CTRL2, 0x27);
		/* Boost Max : 5'b10100 (8.53V) */
		regmap_write(sma1305->regmap, SMA1305_AB_BOOST_CTRL4, 0x14);
		/* Release Time : 88.54us */
		regmap_write(sma1305->regmap, SMA1305_AD_BOOST_CTRL6, 0x10);
		break;
	case SMA1305_RECEIVER_0P5W_MODE:
		/* SPK Volume : -1.5dB */
		regmap_write(sma1305->regmap, SMA1305_0A_SPK_VOL, 0x33);
		/* Shoot Through Protection : Enable */
		regmap_write(sma1305->regmap, SMA1305_0B_BST_TEST, 0xD0);
		/* VBAT & Temperature Sensing Off, LPF Bypass */
		regmap_write(sma1305->regmap,
				SMA1305_0F_VBAT_TEMP_SENSING, 0xE8);
		/* Delay Off */
		regmap_write(sma1305->regmap, SMA1305_13_DELAY, 0x19);
		/* HYSFB : 414kHz, BDELAY : 6'b011100 */
		regmap_write(sma1305->regmap, SMA1305_14_MODULATOR, 0x5C);
		/* Tone Generator(Volume - Off) & Fine volume Bypass */
		regmap_write(sma1305->regmap, SMA1305_1E_TONE_GENERATOR, 0xE1);
		/* Limiter Attack Level : 0.3ms, Release Time : 0.1s */
		regmap_write(sma1305->regmap, SMA1305_24_COMPLIM2, 0x04);
		/* OP1 : 40uA(LOW_PWR), OP2 : 30uA, Low R(10kohm), RCVx1.1 */
		regmap_write(sma1305->regmap, SMA1305_35_FDPEC_CTRL0, 0x45);
		/* ENV_TRA, BOP_CTRL power down */
		regmap_write(sma1305->regmap, SMA1305_3E_IDLE_MODE_CTRL, 0x07);
		/* OTA GM : 10uA/V */
		regmap_write(sma1305->regmap, SMA1305_8F_ANALOG_TEST, 0x00);
		/* FLT_VDD_GAIN : 3.20V */
		regmap_write(sma1305->regmap, SMA1305_92_FDPEC_CTRL1, 0xC0);
		/* Switching Off Slew : 2.6ns, Switching Slew : 4.8ns,
		 * Ramp Compensation : 4.0A/us
		 */
		regmap_write(sma1305->regmap, SMA1305_94_BOOST_CTRL9, 0x91);
		/* High P-gain, OCL : 5.1A */
		regmap_write(sma1305->regmap, SMA1305_95_BOOST_CTRL10, 0x74);
		/* Driver On Deadtime : 2.1ns, Driver Off Deadtime : 2.1ns */
		regmap_write(sma1305->regmap, SMA1305_96_BOOST_CTRL11, 0xFF);
		/* Min V : 5'b00101 (0.53V) */
		regmap_write(sma1305->regmap, SMA1305_A8_BOOST_CTRL1, 0x05);
		/* HEAD_ROOM : 5'b00111 (0.747V) */
		regmap_write(sma1305->regmap, SMA1305_A9_BOOST_CTRL2, 0x27);
		/* Boost Max : 5'b10100 (8.53V) */
		regmap_write(sma1305->regmap, SMA1305_AB_BOOST_CTRL4, 0x14);
		/* Release Time : 88.54us */
		regmap_write(sma1305->regmap, SMA1305_AD_BOOST_CTRL6, 0x10);
		break;
	case SMA1305_SPEAKER_MODE:
	default:
		/* SPK Volume : -1.0dB */
		regmap_write(sma1305->regmap, SMA1305_0A_SPK_VOL, 0x32);
		/* Shoot Through Protection : Disable */
		regmap_write(sma1305->regmap, SMA1305_0B_BST_TEST, 0x50);
		/* VBAT & Temperature Sensing On, LPF Activate */
		regmap_write(sma1305->regmap,
				SMA1305_0F_VBAT_TEMP_SENSING, 0x08);
		/* Delay On - 200us */
		regmap_write(sma1305->regmap, SMA1305_13_DELAY, 0x09);
		/* HYSFB : 625kHz, BDELAY : 6'b010010 */
		regmap_write(sma1305->regmap, SMA1305_14_MODULATOR, 0x12);
		/* Tone Generator(Volume - Off) & Fine volume Activate */
		regmap_write(sma1305->regmap, SMA1305_1E_TONE_GENERATOR, 0xA1);
		/* Limiter Attack Level : 4.7ms, Release Time : 0.45s */
		regmap_write(sma1305->regmap, SMA1305_24_COMPLIM2, 0x7A);
		/* OP1 : 20uA(LOW_PWR), OP2 : 40uA, Low R(10kohm), SPKx3.0 */
		regmap_write(sma1305->regmap, SMA1305_35_FDPEC_CTRL0, 0x16);
		/* ENV_TRA, BOP_CTRL Enable */
		regmap_write(sma1305->regmap, SMA1305_3E_IDLE_MODE_CTRL, 0x01);
		/* OTA GM : 20uA/V */
		regmap_write(sma1305->regmap, SMA1305_8F_ANALOG_TEST, 0x02);
		/* FLT_VDD_GAIN : 3.15V */
		regmap_write(sma1305->regmap, SMA1305_92_FDPEC_CTRL1, 0xB0);
		/* Switching Off Slew : 2.6ns, Switching Slew : 2.6ns,
		 * Ramp Compensation : 7.0A/us
		 */
		regmap_write(sma1305->regmap, SMA1305_94_BOOST_CTRL9, 0xA4);
		/* High P-gain, OCL : 4.0A */
		regmap_write(sma1305->regmap, SMA1305_95_BOOST_CTRL10, 0x54);
		/* Driver On Deadtime : 9.0ns, Driver Off Deadtime : 7.3ns */
		regmap_write(sma1305->regmap, SMA1305_96_BOOST_CTRL11, 0x57);
		/* Min V : 5'b00101 (0.59V) */
		regmap_write(sma1305->regmap, SMA1305_A8_BOOST_CTRL1, 0x04);
		/* HEAD_ROOM : 5'b01000 (1.327V) */
		regmap_write(sma1305->regmap, SMA1305_A9_BOOST_CTRL2, 0x29);
		/* Boost Max : 5'b10001 (10.03V) */
		regmap_write(sma1305->regmap, SMA1305_AB_BOOST_CTRL4, 0x11);
		/* Release Time : 83.33us */
		regmap_write(sma1305->regmap, SMA1305_AD_BOOST_CTRL6, 0x0F);
		break;
	}

	return 0;
}

static int sma1305_startup(struct snd_soc_component *component)
{
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	if (sma1305->amp_power_status) {
		dev_info(component->dev, "%s : %s\n",
			__func__, "Already AMP Power on");
		return 0;
	}

	dev_info(component->dev, "%s\n", __func__);

	regmap_update_bits(sma1305->regmap, SMA1305_A2_TOP_MAN1,
			PLL_MASK, PLL_ON);

	regmap_update_bits(sma1305->regmap, SMA1305_10_SYSTEM_CTRL1,
			SPK_MODE_MASK, SPK_MONO);

	regmap_update_bits(sma1305->regmap, SMA1305_00_SYSTEM_CTRL,
			POWER_MASK, POWER_ON);

	regmap_update_bits(sma1305->regmap, SMA1305_0E_MUTE_VOL_CTRL,
			SPK_MUTE_MASK, SPK_UNMUTE);

	sma1305->amp_power_status = true;

	regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
				DIS_INT_MASK, NORMAL_INT);

	if (sma1305->isr_manual_mode) {
		regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
					CLR_INT_MASK, INT_CLEAR);
		regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
					CLR_INT_MASK, INT_READY);
		regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
				SEL_INT_MASK, INT_CLEAR_MANUAL);
	}
	return 0;
}

static int sma1305_shutdown(struct snd_soc_component *component)
{
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	if (!(sma1305->amp_power_status)) {
		dev_info(component->dev, "%s : %s\n",
			__func__, "Already AMP Shutdown");
		return 0;
	}

	dev_info(component->dev, "%s\n", __func__);

	if (atomic_read(&sma1305->irq_enabled)) {
		disable_irq((unsigned int)sma1305->irq);
		atomic_set(&sma1305->irq_enabled, false);
	}

	regmap_update_bits(sma1305->regmap, SMA1305_0E_MUTE_VOL_CTRL,
			SPK_MUTE_MASK, SPK_MUTE);

	/* To improve the Boost OCP issue,
	 * time should be available for the Boost release time(40ms)
	 * and Mute slope time(15ms)
	 */
	msleep(55);

	regmap_update_bits(sma1305->regmap, SMA1305_10_SYSTEM_CTRL1,
			SPK_MODE_MASK, SPK_OFF);

	regmap_update_bits(sma1305->regmap, SMA1305_00_SYSTEM_CTRL,
			POWER_MASK, POWER_OFF);

	regmap_update_bits(sma1305->regmap, SMA1305_A2_TOP_MAN1,
			PLL_MASK, PLL_OFF);

	sma1305->amp_power_status = false;

	regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
			SEL_INT_MASK, INT_CLEAR_AUTO);
	regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
			DIS_INT_MASK, HIGH_Z_INT);

	return 0;
}

static int sma1305_clk_supply_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_info(component->dev, "%s : PRE_PMU\n", __func__);
	break;

	case SND_SOC_DAPM_POST_PMD:
		dev_info(component->dev, "%s : POST_PMD\n", __func__);
	break;
	}

	return 0;
}

static int sma1305_dac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_info(component->dev, "%s : PRE_PMU\n", __func__);

		if (sma1305->force_amp_power_down == false)
			sma1305_startup(component);

		break;

	case SND_SOC_DAPM_POST_PMU:
		/*dev_info(component->dev, "%s : POST_PMU\n", __func__);*/

		break;

	case SND_SOC_DAPM_PRE_PMD:
		dev_info(component->dev, "%s : PRE_PMD\n", __func__);

		sma1305_shutdown(component);

		break;

	case SND_SOC_DAPM_POST_PMD:
		/*dev_info(component->dev, "%s : POST_PMD\n", __func__);*/

		break;
	}

	return 0;
}

static int sma1305_dac_feedback_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	if (!(sma1305->sdo_bypass_flag)) {
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			dev_info(component->dev,
				"%s : DAC feedback ON\n", __func__);
			regmap_update_bits(sma1305->regmap,
				SMA1305_09_OUTPUT_CTRL,
				PORT_CONFIG_MASK, OUTPUT_PORT_ENABLE);
			regmap_update_bits(sma1305->regmap,
				SMA1305_A3_TOP_MAN2,
				SDO_OUTPUT_MASK, LOGIC_OUTPUT);
			break;

		case SND_SOC_DAPM_PRE_PMD:
			dev_info(component->dev,
				"%s : DAC feedback OFF\n", __func__);
			regmap_update_bits(sma1305->regmap,
				SMA1305_09_OUTPUT_CTRL,
				PORT_CONFIG_MASK, INPUT_PORT_ONLY);
			regmap_update_bits(sma1305->regmap,
				SMA1305_A3_TOP_MAN2,
				SDO_OUTPUT_MASK, HIGH_Z_OUTPUT);
			break;
		}
	} else
		dev_info(component->dev,
			"%s : Force SDO Setting Bypass\n", __func__);

	return 0;
}

static const struct snd_soc_dapm_widget sma1305_dapm_widgets[] = {
SND_SOC_DAPM_SUPPLY("CLK_SUPPLY", SND_SOC_NOPM, 0, 0, sma1305_clk_supply_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_DAC_E("DAC", "Playback", SND_SOC_NOPM, 0, 0, sma1305_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_ADC_E("DAC_FEEDBACK", "Capture", SND_SOC_NOPM, 0, 0,
				sma1305_dac_feedback_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_OUTPUT("SPK"),
SND_SOC_DAPM_INPUT("SDO"),
};

static const struct snd_soc_dapm_route sma1305_audio_map[] = {
/* sink, control, source */
{"DAC", NULL, "CLK_SUPPLY"},
{"SPK", NULL, "DAC"},
{"DAC_FEEDBACK", NULL, "SDO"},
};

static int sma1305_setup_pll(struct snd_soc_component *component,
		unsigned int bclk)
{
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	int i = 0;

	dev_info(component->dev, "%s : BCLK = %dHz\n",
		__func__, bclk);

	if (sma1305->sys_clk_id == SMA1305_PLL_CLKIN_MCLK) {
		dev_info(component->dev, "%s : MCLK is not supported\n",
		__func__);
	} else if (sma1305->sys_clk_id == SMA1305_PLL_CLKIN_BCLK) {
		for (i = 0; i < sma1305->num_of_pll_matches; i++) {
			if (sma1305->pll_matches[i].input_clk == bclk)
				break;
		}
		if (i == sma1305->num_of_pll_matches) {
			dev_info(component->dev, "%s : No matching value between pll table and SCK\n",
				__func__);
			return -EINVAL;
		}

		/* PLL operation, PLL Clock, External Clock,
		 * PLL reference SCK clock
		 */
		regmap_update_bits(sma1305->regmap, SMA1305_A2_TOP_MAN1,
				PLL_MASK, PLL_ON);

	}

	regmap_write(sma1305->regmap, SMA1305_8B_PLL_POST_N,
			sma1305->pll_matches[i].post_n);
	regmap_write(sma1305->regmap, SMA1305_8C_PLL_N,
			sma1305->pll_matches[i].n);
	regmap_write(sma1305->regmap, SMA1305_8D_PLL_A_SETTING,
			sma1305->pll_matches[i].vco);
	regmap_write(sma1305->regmap, SMA1305_8E_PLL_P_CP,
			sma1305->pll_matches[i].p_cp);

	return 0;
}

static int sma1305_dai_hw_params_amp(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	unsigned int input_format = 0;
	unsigned int bclk = 0;

	if (sma1305->format == SND_SOC_DAIFMT_DSP_A)
		bclk = params_rate(params) * sma1305->frame_size;
	else
		bclk = params_rate(params) * params_physical_width(params)
			* params_channels(params);

	dev_info(component->dev,
			"%s : rate = %d : bit size = %d : channel = %d\n",
			__func__, params_rate(params), params_width(params),
			params_channels(params));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		/* PLL clock setting according to sample rate and bit */
		if (sma1305->force_amp_power_down == false &&
			(sma1305->sys_clk_id == SMA1305_PLL_CLKIN_MCLK
			|| sma1305->sys_clk_id == SMA1305_PLL_CLKIN_BCLK)) {

			if (sma1305->last_bclk != bclk) {
				if (sma1305->amp_power_status) {
					sma1305_shutdown(component);
					sma1305_setup_pll(component, bclk);
					sma1305_startup(component);
				} else
					sma1305_setup_pll(component, bclk);

				sma1305->last_bclk = bclk;
			}
		}

		if (sma1305->force_amp_power_down == false &&
			!atomic_read(&sma1305->irq_enabled)) {
			enable_irq((unsigned int)sma1305->irq);
			irq_set_irq_wake(sma1305->irq, 1);

			if (device_may_wakeup(sma1305->dev))
				enable_irq_wake(sma1305->irq);

			atomic_set(&sma1305->irq_enabled, true);
		}

		switch (params_rate(params)) {
		case 8000:
		case 12000:
		case 16000:
		case 24000:
		case 32000:
		case 44100:
		case 48000:
			sma1305->sdo_bypass_flag = false;
			break;

		case 96000:
			dev_info(component->dev, "%s : %d rate not support SDO\n",
				__func__, params_rate(params));
			sma1305->sdo_bypass_flag = true;
			break;

		default:
			dev_err(component->dev, "%s not support rate : %d\n",
				__func__, params_rate(params));
			sma1305->sdo_bypass_flag = true;

		return -EINVAL;
		}

	/* substream->stream is SNDRV_PCM_STREAM_CAPTURE */
	} else {

		switch (params_format(params)) {

		case SNDRV_PCM_FORMAT_S16_LE:
			dev_info(component->dev,
				"%s set format SNDRV_PCM_FORMAT_S16_LE\n",
				__func__);
			regmap_update_bits(sma1305->regmap, SMA1305_A4_TOP_MAN3,
					SCK_RATE_MASK, SCK_32FS);
			regmap_update_bits(sma1305->regmap, SMA1305_A4_TOP_MAN3,
					DATA_WIDTH_MASK, DATA_16BIT);
			break;

		case SNDRV_PCM_FORMAT_S24_LE:
			dev_info(component->dev,
				"%s set format SNDRV_PCM_FORMAT_S24_LE\n",
				__func__);
			regmap_update_bits(sma1305->regmap, SMA1305_A4_TOP_MAN3,
					SCK_RATE_MASK, SCK_64FS);
			regmap_update_bits(sma1305->regmap, SMA1305_A4_TOP_MAN3,
					DATA_WIDTH_MASK, DATA_24BIT);
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			dev_info(component->dev,
				"%s set format SNDRV_PCM_FORMAT_S32_LE\n",
				__func__);
			regmap_update_bits(sma1305->regmap, SMA1305_A4_TOP_MAN3,
					SCK_RATE_MASK, SCK_64FS);
			regmap_update_bits(sma1305->regmap, SMA1305_A4_TOP_MAN3,
					DATA_WIDTH_MASK, DATA_24BIT);
			break;
		default:
			dev_err(component->dev,
				"%s not support data bit : %d\n", __func__,
						params_format(params));
			return -EINVAL;

		}
	}

	switch (params_width(params)) {
	case 16:
		switch (sma1305->format) {
		case SND_SOC_DAIFMT_I2S:
			input_format |= STANDARD_I2S;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			input_format |= LJ;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			input_format |= RJ_16BIT;
			break;
		}
		break;
	case 24:
		switch (sma1305->format) {
		case SND_SOC_DAIFMT_I2S:
			input_format |= STANDARD_I2S;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			input_format |= LJ;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			input_format |= RJ_24BIT;
			break;
		}
		break;
	case 32:
		switch (sma1305->format) {
		case SND_SOC_DAIFMT_I2S:
			input_format |= STANDARD_I2S;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			input_format |= LJ;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			input_format |= RJ_24BIT;
			break;
		}
		break;


	default:
		dev_err(component->dev,
			"%s not support data bit : %d\n", __func__,
					params_format(params));
		return -EINVAL;
	}

	regmap_update_bits(sma1305->regmap, SMA1305_01_INPUT_CTRL1,
				I2S_MODE_MASK, input_format);

	return 0;
}

static int sma1305_dai_set_sysclk_amp(struct snd_soc_dai *dai,
				int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = dai->component;
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s\n", __func__);

	switch (clk_id) {
	case SMA1305_EXTERNAL_CLOCK_19_2:
		break;
	case SMA1305_EXTERNAL_CLOCK_24_576:
		break;
	case SMA1305_PLL_CLKIN_MCLK:
		break;
	case SMA1305_PLL_CLKIN_BCLK:
		break;
	default:
		dev_err(component->dev, "Invalid clk id: %d\n", clk_id);
		return -EINVAL;
	}
	sma1305->sys_clk_id = clk_id;
	return 0;
}

static int sma1305_dai_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_component *component = dai->component;
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	if (!(sma1305->amp_power_status)) {
		dev_info(component->dev, "%s : %s\n",
			__func__, "Already AMP Shutdown");
		return 0;
	}

	if (mute) {
		dev_info(component->dev, "%s : %s\n", __func__, "MUTE");

		regmap_update_bits(sma1305->regmap, SMA1305_0E_MUTE_VOL_CTRL,
					SPK_MUTE_MASK, SPK_MUTE);
	} else {
		dev_info(component->dev, "%s : %s\n", __func__, "UNMUTE");

		regmap_update_bits(sma1305->regmap, SMA1305_0E_MUTE_VOL_CTRL,
					SPK_MUTE_MASK, SPK_UNMUTE);
	}

	return 0;
}

static int sma1305_dai_set_fmt_amp(struct snd_soc_dai *dai,
					unsigned int fmt)
{
	struct snd_soc_component *component  = dai->component;
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {

	case SND_SOC_DAIFMT_CBS_CFS:
		dev_info(component->dev,
			"%s : %s\n", __func__, "I2S/TDM Device mode");
		/* I2S/PCM clock mode - Device mode */
		regmap_update_bits(sma1305->regmap, SMA1305_01_INPUT_CTRL1,
					CONTROLLER_DEVICE_MASK, DEVICE_MODE);

		break;

	case SND_SOC_DAIFMT_CBM_CFM:
		dev_info(component->dev,
			"%s : %s\n", __func__, "I2S/TDM Controller mode");
		/* I2S/PCM clock mode - Controller mode */
		regmap_update_bits(sma1305->regmap, SMA1305_01_INPUT_CTRL1,
				CONTROLLER_DEVICE_MASK, CONTROLLER_MODE);
		break;

	default:
		dev_err(component->dev,
				"Unsupported Controller/Device : 0x%x\n", fmt);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		sma1305->format = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		break;

	default:
		dev_err(component->dev,
			"Unsupported Audio INterface FORMAT : 0x%x\n", fmt);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {

	case SND_SOC_DAIFMT_IB_NF:
		dev_info(component->dev, "%s : %s\n",
			__func__, "Invert BCLK + Normal Frame");
		regmap_update_bits(sma1305->regmap, SMA1305_01_INPUT_CTRL1,
					SCK_RISING_MASK, SCK_RISING_EDGE);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		dev_info(component->dev, "%s : %s\n",
			__func__, "Invert BCLK + Invert Frame");
		regmap_update_bits(sma1305->regmap, SMA1305_01_INPUT_CTRL1,
					LEFTPOL_MASK|SCK_RISING_MASK,
					HIGH_FIRST_CH|SCK_RISING_EDGE);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		dev_info(component->dev, "%s : %s\n",
			__func__, "Normal BCLK + Invert Frame");
		regmap_update_bits(sma1305->regmap, SMA1305_01_INPUT_CTRL1,
					LEFTPOL_MASK, HIGH_FIRST_CH);
		break;
	case SND_SOC_DAIFMT_NB_NF:
		dev_info(component->dev, "%s : %s\n",
			__func__, "Normal BCLK + Normal Frame");
		break;
	default:
		dev_err(component->dev,
				"Unsupported Bit & Frameclock : 0x%x\n", fmt);
		return -EINVAL;
	}

	return 0;
}

static int sma1305_dai_set_tdm_slot(struct snd_soc_dai *dai,
				   unsigned int tx_mask, unsigned int rx_mask,
				   int slots, int slot_width)
{
	struct snd_soc_component *component  = dai->component;
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s : slots - %d, slot_width - %d\n",
			__func__, slots, slot_width);

	sma1305->frame_size = slot_width * slots;

	regmap_update_bits(sma1305->regmap, SMA1305_A4_TOP_MAN3,
		INTERFACE_MASK, TDM_FORMAT);

	switch (slot_width) {
	case 16:
	regmap_update_bits(sma1305->regmap, SMA1305_A6_TDM2,
			TDM_DL_MASK, TDM_DL_16);
	break;
	case 32:
	regmap_update_bits(sma1305->regmap, SMA1305_A6_TDM2,
			TDM_DL_MASK, TDM_DL_32);
	break;

	default:
	dev_err(component->dev, "%s not support TDM %d slot_width\n",
		__func__, slot_width);
	}

	switch (slots) {
	case 4:
	regmap_update_bits(sma1305->regmap, SMA1305_A6_TDM2,
			TDM_N_SLOT_MASK, TDM_N_SLOT_4);
	break;
	case 8:
	regmap_update_bits(sma1305->regmap, SMA1305_A6_TDM2,
			TDM_N_SLOT_MASK, TDM_N_SLOT_8);
	break;
	default:
	dev_err(component->dev, "%s not support TDM %d slots\n",
		__func__, slots);
	}

	/* Select a slot to process TDM Rx data */
	if (sma1305->tdm_slot_rx < slots)
		regmap_update_bits(sma1305->regmap,
			SMA1305_A5_TDM1, TDM_SLOT1_RX_POS_MASK,
			(sma1305->tdm_slot_rx) << 3);
	else
		dev_err(component->dev, "%s Incorrect tdm-slot-rx %d set\n",
			__func__, sma1305->tdm_slot_rx);

	regmap_update_bits(sma1305->regmap, SMA1305_A5_TDM1,
			TDM_TX_MODE_MASK, TDM_TX_MONO);
	/* Select a slot to process TDM Tx data */
	if (sma1305->tdm_slot_tx < slots)
		regmap_update_bits(sma1305->regmap,
			SMA1305_A6_TDM2, TDM_SLOT1_TX_POS_MASK,
			(sma1305->tdm_slot_tx) << 3);
	else
		dev_err(component->dev, "%s Incorrect tdm-slot-tx %d set\n",
			__func__, sma1305->tdm_slot_tx);

	return 0;
}

static const struct snd_soc_dai_ops sma1305_dai_ops_amp = {
	.set_sysclk = sma1305_dai_set_sysclk_amp,
	.set_fmt = sma1305_dai_set_fmt_amp,
	.hw_params = sma1305_dai_hw_params_amp,
	.digital_mute = sma1305_dai_digital_mute,
	.set_tdm_slot = sma1305_dai_set_tdm_slot,
};

#define SMA1305_RATES_PLAYBACK SNDRV_PCM_RATE_8000_96000
#define SMA1305_RATES_CAPTURE SNDRV_PCM_RATE_8000_48000
#define SMA1305_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
		SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver sma1305_dai[] = {
{
	.name = "sma1305-amplifier",
	.id = 0,
	.playback = {
	.stream_name = "Playback",
	.channels_min = 1,
	.channels_max = 2,
	.rates = SMA1305_RATES_PLAYBACK,
	.formats = SMA1305_FORMATS,
	},
	.capture = {
	.stream_name = "Capture",
	.channels_min = 1,
	.channels_max = 2,
	.rates = SMA1305_RATES_CAPTURE,
	.formats = SMA1305_FORMATS,
	},
	.ops = &sma1305_dai_ops_amp,
}
};

static int sma1305_set_bias_level(struct snd_soc_component *component,
				enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:

		dev_info(component->dev, "%s\n", "SND_SOC_BIAS_ON");
		sma1305_startup(component);

		break;

	case SND_SOC_BIAS_PREPARE:

		dev_info(component->dev, "%s\n", "SND_SOC_BIAS_PREPARE");

		break;

	case SND_SOC_BIAS_STANDBY:

		dev_info(component->dev, "%s\n", "SND_SOC_BIAS_STANDBY");

		break;

	case SND_SOC_BIAS_OFF:

		dev_info(component->dev, "%s\n", "SND_SOC_BIAS_OFF");
		sma1305_shutdown(component);

		break;
	}

	return 0;
}

static irqreturn_t sma1305_isr(int irq, void *data)
{
	struct sma1305_priv *sma1305 = (struct sma1305_priv *) data;

	if (sma1305->check_fault_status)
		queue_delayed_work(system_freezable_wq,
				&sma1305->check_fault_work, 0);

	if (sma1305->isr_manual_mode) {
		regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
					CLR_INT_MASK, INT_CLEAR);
		regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
					CLR_INT_MASK, INT_READY);
	}
	return IRQ_HANDLED;
}

static void sma1305_check_fault_worker(struct work_struct *work)
{
	struct sma1305_priv *sma1305 =
		container_of(work, struct sma1305_priv, check_fault_work.work);
	int ret;
	unsigned int status1_val, status2_val;

	mutex_lock(&sma1305->lock);

	dev_info(sma1305->dev, "%s\n", __func__);

	if (sma1305->tsdw_cnt)
		ret = regmap_read(sma1305->regmap,
			SMA1305_0A_SPK_VOL, &sma1305->cur_vol);
	else
		ret = regmap_read(sma1305->regmap,
			SMA1305_0A_SPK_VOL, &sma1305->init_vol);

	if (ret != 0) {
		dev_err(sma1305->dev,
			"failed to read SMA1305_0A_SPK_VOL : %d\n", ret);
		mutex_unlock(&sma1305->lock);
		return;
	}

	ret = regmap_read(sma1305->regmap, SMA1305_FA_STATUS1, &status1_val);
	if (ret != 0) {
		dev_err(sma1305->dev,
			"failed to read SMA1305_FA_STATUS1 : %d\n", ret);
		mutex_unlock(&sma1305->lock);
		return;
	}

	ret = regmap_read(sma1305->regmap, SMA1305_FB_STATUS2, &status2_val);
	if (ret != 0) {
		dev_err(sma1305->dev,
			"failed to read SMA1305_FB_STATUS2 : %d\n", ret);
		mutex_unlock(&sma1305->lock);
		return;
	}

	if (~status1_val & OT1_OK_STATUS) {
		dev_crit(sma1305->dev,
			"%s : OT1(Over Temperature Level 1)\n", __func__);
		/* Volume control (Current Volume -3dB) */
		if ((sma1305->cur_vol + 6) <= 0xFF)
			regmap_write(sma1305->regmap,
				SMA1305_0A_SPK_VOL, sma1305->cur_vol + 6);

		if (sma1305->check_fault_period > 0)
			queue_delayed_work(system_freezable_wq,
				&sma1305->check_fault_work,
					sma1305->check_fault_period * HZ);
		else
			queue_delayed_work(system_freezable_wq,
				&sma1305->check_fault_work,
					CHECK_PERIOD_TIME * HZ);
		sma1305->tsdw_cnt++;
	} else if (sma1305->tsdw_cnt) {
		regmap_write(sma1305->regmap,
			SMA1305_0A_SPK_VOL, sma1305->init_vol);
		sma1305->tsdw_cnt = 0;
		sma1305->cur_vol = sma1305->init_vol;
	}

	if (~status1_val & OT2_OK_STATUS) {
		dev_crit(sma1305->dev,
			"%s : OT2(Over Temperature Level 2)\n", __func__);
	}
	if (status1_val & UVLO_STATUS) {
		dev_crit(sma1305->dev,
			"%s : UVLO(Under Voltage Lock Out)\n", __func__);
	}
	if (status1_val & OVP_BST_STATUS) {
		dev_crit(sma1305->dev,
			"%s : OVP_BST(Over Voltage Protection)\n", __func__);
	}
	if (status2_val & OCP_SPK_STATUS) {
		dev_crit(sma1305->dev,
			"%s : OCP_SPK(Over Current Protect SPK)\n", __func__);
	}
	if (status2_val & OCP_BST_STATUS) {
		dev_crit(sma1305->dev,
			"%s : OCP_BST(Over Current Protect Boost)\n", __func__);
	}
	if ((status2_val & CLK_MON_STATUS) && (sma1305->amp_power_status)) {
		dev_crit(sma1305->dev,
			"%s : CLK_FAULT(No clock input)\n", __func__);
	}

	mutex_unlock(&sma1305->lock);

}

#ifdef CONFIG_PM
static int sma1305_suspend(struct snd_soc_component *component)
{
	dev_info(component->dev, "%s\n", __func__);

	return 0;
}

static int sma1305_resume(struct snd_soc_component *component)
{
	dev_info(component->dev, "%s\n", __func__);

	return 0;
}
#else
#define sma1305_suspend NULL
#define sma1305_resume NULL
#endif

static int sma1305_reset(struct snd_soc_component *component)
{
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);
	int ret;
	unsigned int status, i;

	dev_info(component->dev, "%s\n", __func__);

	ret = regmap_read(sma1305->regmap, SMA1305_FF_DEVICE_INDEX, &status);

	if (ret != 0)
		dev_err(sma1305->dev,
			"failed to read SMA1305_FF_DEVICE_INDEX : %d\n", ret);
	else {
		sma1305->rev_num = status & REV_NUM_STATUS;
		dev_info(component->dev,
				"SMA1305 Revision %d\n", sma1305->rev_num);
	}

	regmap_read(sma1305->regmap, SMA1305_99_OTP_TRM2, &sma1305->otp_trm2);
	regmap_read(sma1305->regmap, SMA1305_9A_OTP_TRM3, &sma1305->otp_trm3);

	if ((sma1305->otp_trm2 & OTP_STAT_MASK) == OTP_STAT_1)
		dev_info(component->dev, "SMA1305 OTP Status Successful\n");
	else
		dev_info(component->dev, "SMA1305 OTP Status Fail\n");

	/* Register Initial Value Setting */
	for (i = 0; i < (unsigned int) ARRAY_SIZE(sma1305_reg_def); i++)
		regmap_write(sma1305->regmap,
			sma1305_reg_def[i].reg, sma1305_reg_def[i].def);
	if (sma1305->rev_num == REV_NUM_REV0) {
		regmap_write(sma1305->regmap, SMA1305_8F_ANALOG_TEST, 0x00);
		regmap_write(sma1305->regmap, SMA1305_92_FDPEC_CTRL1, 0x80);
		regmap_write(sma1305->regmap, SMA1305_95_BOOST_CTRL10, 0x74);
		regmap_write(sma1305->regmap, SMA1305_A8_BOOST_CTRL1, 0x05);
		regmap_write(sma1305->regmap, SMA1305_A9_BOOST_CTRL2, 0x28);
		regmap_write(sma1305->regmap, SMA1305_AB_BOOST_CTRL4, 0x14);
		regmap_write(sma1305->regmap, SMA1305_99_OTP_TRM2, 0x00);
		regmap_update_bits(sma1305->regmap, SMA1305_9A_OTP_TRM3,
				RCV_OFFS2_MASK, RCV_OFFS2_DEFAULT_VALUE);
	}
	regmap_update_bits(sma1305->regmap, SMA1305_93_INT_CTRL,
			DIS_INT_MASK, HIGH_Z_INT);
	switch (sma1305->sdo_ch) {
	case SMA1305_SDO_TWO_CH_24:
		regmap_update_bits(sma1305->regmap, SMA1305_A2_TOP_MAN1,
			SDO_OUTPUT2_MASK, TWO_SDO_PER_CH);
		regmap_update_bits(sma1305->regmap, SMA1305_A3_TOP_MAN2,
			SDO_OUTPUT3_MASK, TWO_SDO_PER_CH_24K);
		break;
	case SMA1305_SDO_TWO_CH_48:
		regmap_update_bits(sma1305->regmap, SMA1305_A2_TOP_MAN1,
			SDO_OUTPUT2_MASK, TWO_SDO_PER_CH);
		regmap_update_bits(sma1305->regmap, SMA1305_A3_TOP_MAN2,
			SDO_OUTPUT3_MASK, SDO_OUTPUT3_DIS);
		break;
	case SMA1305_SDO_ONE_CH:
	default:
		regmap_update_bits(sma1305->regmap, SMA1305_A2_TOP_MAN1,
			SDO_OUTPUT2_MASK, ONE_SDO_PER_CH);
		regmap_update_bits(sma1305->regmap, SMA1305_A3_TOP_MAN2,
			SDO_OUTPUT3_MASK, SDO_OUTPUT3_DIS);
		break;
	}
	regmap_update_bits(sma1305->regmap, SMA1305_09_OUTPUT_CTRL,
			SDO_OUT0_SEL_MASK, sma1305->sdo0_sel);
	regmap_update_bits(sma1305->regmap, SMA1305_09_OUTPUT_CTRL,
			SDO_OUT1_SEL_MASK, sma1305->sdo1_sel);
	regmap_write(sma1305->regmap, SMA1305_0A_SPK_VOL, sma1305->init_vol);

	if (sma1305->stereo_two_chip == true) {
		/* MONO MIX Off */
		regmap_update_bits(sma1305->regmap,
		SMA1305_11_SYSTEM_CTRL2, MONOMIX_MASK, MONOMIX_OFF);
	} else {
		/* MONO MIX On */
		regmap_update_bits(sma1305->regmap,
		SMA1305_11_SYSTEM_CTRL2, MONOMIX_MASK, MONOMIX_ON);
	}

	return 0;
}

int get_sma_amp_component(struct snd_soc_component **component)
{
	*component = sma1305_amp_component;
	return 0;
}
EXPORT_SYMBOL(get_sma_amp_component);

#ifdef CONFIG_SMA1305_FACTORY_RECOVERY_SYSFS
int sma1305_reinit(struct snd_soc_component *component)
{
	sma1305_reset(component);
	sma1305_spk_rcv_conf(component);

	return 0;
}
EXPORT_SYMBOL(sma1305_reinit);
#endif

static ssize_t check_fault_period_show(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct sma1305_priv *sma1305 = dev_get_drvdata(dev);
	int rc;

	rc = (int)snprintf(buf, PAGE_SIZE,
			"%ld\n", sma1305->check_fault_period);

	return (ssize_t)rc;
}

static ssize_t check_fault_period_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sma1305_priv *sma1305 = dev_get_drvdata(dev);
	int ret;

	ret = kstrtol(buf, 10, &sma1305->check_fault_period);

	if (ret)
		return -EINVAL;

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(check_fault_period);

static ssize_t check_fault_status_show(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct sma1305_priv *sma1305 = dev_get_drvdata(dev);
	int rc;

	rc = (int)snprintf(buf, PAGE_SIZE,
			"%ld\n", sma1305->check_fault_status);

	return (ssize_t)rc;
}

static ssize_t check_fault_status_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sma1305_priv *sma1305 = dev_get_drvdata(dev);
	int ret;

	ret = kstrtol(buf, 10, &sma1305->check_fault_status);

	if (ret)
		return -EINVAL;

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(check_fault_status);

static ssize_t isr_manual_mode_show(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct sma1305_priv *sma1305 = dev_get_drvdata(dev);
	int rc;

	rc = (int)snprintf(buf, PAGE_SIZE,
			"%ld\n", sma1305->isr_manual_mode);

	return (ssize_t)rc;
}

static ssize_t isr_manual_mode_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sma1305_priv *sma1305 = dev_get_drvdata(dev);
	int ret;

	ret = kstrtol(buf, 10, &sma1305->isr_manual_mode);

	if (ret)
		return -EINVAL;

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(isr_manual_mode);

static struct attribute *sma1305_attr[] = {
	&dev_attr_check_fault_period.attr,
	&dev_attr_check_fault_status.attr,
	&dev_attr_isr_manual_mode.attr,
	NULL,
};

static struct attribute_group sma1305_attr_group = {
	.attrs = sma1305_attr,
};

static int sma1305_probe(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	char *dapm_widget_str = NULL;
	int prefix_len = 0, str_max = 30;

	dev_info(component->dev, "%s\n", __func__);

	if (component->name_prefix != NULL) {
		dev_info(component->dev, "%s : component name prefix - %s\n",
			__func__, component->name_prefix);

		prefix_len = strlen(component->name_prefix);
		dapm_widget_str = kzalloc(prefix_len + str_max, GFP_KERNEL);

		if (!dapm_widget_str) {
			kfree(dapm_widget_str);
			return -ENOMEM;
		}

		strcpy(dapm_widget_str, component->name_prefix);
		strcat(dapm_widget_str, " Playback");

		snd_soc_dapm_ignore_suspend(dapm, dapm_widget_str);

		memset(dapm_widget_str + prefix_len, 0, str_max);

		strcpy(dapm_widget_str, component->name_prefix);
		strcat(dapm_widget_str, " SPK");

		snd_soc_dapm_ignore_suspend(dapm, dapm_widget_str);

		kfree(dapm_widget_str);
	} else {
		snd_soc_dapm_ignore_suspend(dapm, "Playback");
		snd_soc_dapm_ignore_suspend(dapm, "SPK");
	}

	snd_soc_dapm_sync(dapm);

	sma1305_amp_component = component;
	sma1305_reset(component);

	return 0;
}

static void sma1305_remove(struct snd_soc_component *component)
{
	struct sma1305_priv *sma1305 = snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s\n", __func__);

	cancel_delayed_work_sync(&sma1305->check_fault_work);
	sma1305_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static const struct snd_soc_component_driver sma1305_component = {
	.probe = sma1305_probe,
	.remove = sma1305_remove,
	.suspend = sma1305_suspend,
	.resume = sma1305_resume,
	.controls = sma1305_snd_controls,
	.num_controls = ARRAY_SIZE(sma1305_snd_controls),
	.dapm_widgets = sma1305_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sma1305_dapm_widgets),
	.dapm_routes = sma1305_audio_map,
	.num_dapm_routes = ARRAY_SIZE(sma1305_audio_map),
};

const struct regmap_config sma_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SMA1305_FF_DEVICE_INDEX,
	.readable_reg = sma1305_readable_register,
	.writeable_reg = sma1305_writeable_register,
	.volatile_reg = sma1305_volatile_register,

	.cache_type = REGCACHE_NONE,
	.reg_defaults = sma1305_reg_def,
	.num_reg_defaults = ARRAY_SIZE(sma1305_reg_def),
};

static int sma1305_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct sma1305_priv *sma1305;
	struct device_node *np = client->dev.of_node;
	int ret;
	u32 value;
	unsigned int device_info;

	dev_info(&client->dev, "%s is here. Driver version REV012\n", __func__);

	sma1305 = devm_kzalloc(&client->dev, sizeof(struct sma1305_priv),
							GFP_KERNEL);

	if (!sma1305)
		return -ENOMEM;

	sma1305->regmap = devm_regmap_init_i2c(client, &sma_i2c_regmap);

	if (IS_ERR(sma1305->regmap)) {
		ret = PTR_ERR(sma1305->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);

		/* Comment out the code due to GKI build error */
//		if (sma1305->regmap)
//			regmap_exit(sma1305->regmap);

		devm_kfree(&client->dev, sma1305);

		return ret;
	}

	if (np) {
		if (!of_property_read_u32(np, "init-vol", &value)) {
			sma1305->init_vol = value;
			dev_info(&client->dev,
				"init_vol is 0x%x from DT\n", value);
		} else {
			dev_info(&client->dev,
				"init_vol is set with 0x32(-1.0dB)\n");
			sma1305->init_vol = 0x32;
		}
		if (of_property_read_bool(np, "stereo-two-chip")) {
			dev_info(&client->dev, "Stereo for two chip solution\n");
				sma1305->stereo_two_chip = true;
		} else {
			dev_info(&client->dev, "Mono for one chip solution\n");
				sma1305->stereo_two_chip = false;
		}
		if (of_property_read_bool(np, "impossible-bst-ctrl")) {
			dev_info(&client->dev, "Boost control setting is not possible\n");
				sma1305->impossible_bst_ctrl = true;
		} else {
			dev_info(&client->dev, "Boost control setting is possible\n");
				sma1305->impossible_bst_ctrl = false;
		}
		if (!of_property_read_u32(np, "tdm-slot-rx", &value)) {
			dev_info(&client->dev,
				"tdm slot rx is '%d' from DT\n", value);
			sma1305->tdm_slot_rx = value;
		} else {
			dev_info(&client->dev,
				"Default setting of tdm slot rx is '0'\n");
			sma1305->tdm_slot_rx = 0;
		}
		if (!of_property_read_u32(np, "tdm-slot-tx", &value)) {
			dev_info(&client->dev,
				"tdm slot tx is '%d' from DT\n", value);
			sma1305->tdm_slot_tx = value;
		} else {
			dev_info(&client->dev,
				"Default setting of tdm slot tx is '0'\n");
			sma1305->tdm_slot_tx = 0;
		}
		if (!of_property_read_u32(np, "sys-clk-id", &value)) {
			switch (value) {
			case SMA1305_EXTERNAL_CLOCK_19_2:
			case SMA1305_EXTERNAL_CLOCK_24_576:
			case SMA1305_PLL_CLKIN_MCLK:
				dev_info(&client->dev, "MCLK is not supported\n");
				break;
			case SMA1305_PLL_CLKIN_BCLK:
				dev_info(&client->dev,
				"Take an BCLK(SCK) and covert it to an internal PLL for use\n");
				break;
			default:
				dev_err(&client->dev,
					"Invalid sys-clk-id: %d\n", value);
				return -EINVAL;
			}
			sma1305->sys_clk_id = value;

		} else {
			dev_info(&client->dev, "Use the internal PLL clock by default\n");
			sma1305->sys_clk_id = SMA1305_PLL_CLKIN_BCLK;
		}
		if (!of_property_read_u32(np, "sdo-ch", &value)) {
			switch (value) {
			case SMA1305_SDO_ONE_CH:
				dev_info(&client->dev,
					"One SDO Output per Channel\n");
				break;
			case SMA1305_SDO_TWO_CH_48:
				dev_info(&client->dev,
					"Two SDO Outputs per Channel_48k\n");
				break;
			case SMA1305_SDO_TWO_CH_24:
				dev_info(&client->dev,
					"Two SDO Outputs per Channel_24k\n");
				break;
			default:
				dev_err(&client->dev,
					"Invalid sdo-ch: %d\n", value);
				return -EINVAL;
			}
			sma1305->sdo_ch = value;
		} else {
			dev_info(&client->dev,
				"One SDO Output per Channel\n");
			sma1305->sdo_ch = SMA1305_SDO_ONE_CH;
		}
		if (!of_property_read_u32(np, "sdo0-sel", &value)) {
			switch (value) {
			case SMA1305_SDO_DISABLE:
				dev_info(&client->dev,
					"SDO0 Output disable\n");
				break;
			case SMA1305_FORMAT_C:
				dev_info(&client->dev,
					"SDO0 Format converter\n");
				break;
			case SMA1305_MONO_MIX:
				dev_info(&client->dev,
					"SDO0 Mono mixer output\n");
				break;
			case SMA1305_AFTER_DSP:
				dev_info(&client->dev,
					"SDO0 Speaker path after DSP\n");
				break;
			case SMA1305_VRMS2_AVG:
				dev_info(&client->dev,
					"SDO0 Vrms^2 average\n");
				break;
			case SMA1305_VBAT_MON:
				dev_info(&client->dev,
					"SDO0 Battery monitoring\n");
				break;
			case SMA1305_TEMP_MON:
				dev_info(&client->dev,
					"SDO0 Temperature monitoring\n");
				break;
			case SMA1305_AFTER_DELAY:
				dev_info(&client->dev,
					"SDO0 Speaker path after Delay\n");
				break;
			default:
				dev_err(&client->dev,
					"Invalid sdo0-sel: %d\n", value);
				return -EINVAL;
			}
			sma1305->sdo0_sel = value;
		} else {
			dev_info(&client->dev,
					"SDO0 Speaker path after Delay\n");
			sma1305->sdo0_sel = SMA1305_AFTER_DELAY;
		}
		if (!of_property_read_u32(np, "sdo1-sel", &value)) {
			switch (value) {
			case SMA1305_SDO_DISABLE:
				dev_info(&client->dev,
					"SDO1 Output disable\n");
				break;
			case SMA1305_FORMAT_C:
				dev_info(&client->dev,
					"SDO1 Format converter\n");
				break;
			case SMA1305_MONO_MIX:
				dev_info(&client->dev,
					"SDO1 Mono mixer output\n");
				break;
			case SMA1305_AFTER_DSP:
				dev_info(&client->dev,
					"SDO1 Speaker path after DSP\n");
				break;
			case SMA1305_VRMS2_AVG:
				dev_info(&client->dev,
					"SDO1 Vrms^2 average\n");
				break;
			case SMA1305_VBAT_MON:
				dev_info(&client->dev,
					"SDO1 Battery monitoring\n");
				break;
			case SMA1305_TEMP_MON:
				dev_info(&client->dev,
					"SDO1 Temperature monitoring\n");
				break;
			case SMA1305_AFTER_DELAY:
				dev_info(&client->dev,
					"SDO1 Speaker path after Delay\n");
				break;
			default:
				dev_err(&client->dev,
					"Invalid sdo1-sel: %d\n", value);
				return -EINVAL;
			}
			sma1305->sdo1_sel = (value<<3);
		} else {
			dev_info(&client->dev,
					"SDO1 Output disable\n");
			sma1305->sdo1_sel = SDO1_DISABLE;
		}
		sma1305->gpio_int = of_get_named_gpio(np,
				"sma1305,gpio-int", 0);
		if (!gpio_is_valid(sma1305->gpio_int)) {
			dev_err(&client->dev,
			"Looking up %s property in node %s failed %d\n",
			"sma1305,gpio-int", client->dev.of_node->full_name,
			sma1305->gpio_int);
		}

	} else {
		dev_err(&client->dev,
			"device node initialization error\n");
		devm_kfree(&client->dev, sma1305);
		return -ENODEV;
	}

	ret = regmap_read(sma1305->regmap,
		SMA1305_FF_DEVICE_INDEX, &device_info);

	if ((ret != 0) || ((device_info & 0xF8) != DEVICE_ID)) {
		dev_err(&client->dev, "device initialization error (%d 0x%02X)",
				ret, device_info);
		devm_kfree(&client->dev, sma1305);
		return -ENODEV;
	}
	dev_info(&client->dev, "chip version 0x%02X\n", device_info);

	/* set initial value as normal AMP IC status */
	sma1305->last_bclk = 0;
	sma1305->tsdw_cnt = 0;
	sma1305->cur_vol = sma1305->init_vol;

	mutex_init(&sma1305->lock);
	INIT_DELAYED_WORK(&sma1305->check_fault_work,
		sma1305_check_fault_worker);
	sma1305->check_fault_period = CHECK_PERIOD_TIME;
	sma1305->check_fault_status = true;
	sma1305->isr_manual_mode = true;

	sma1305->devtype = (enum sma1305_type) id->driver_data;
	sma1305->dev = &client->dev;
	sma1305->kobj = &client->dev.kobj;

	i2c_set_clientdata(client, sma1305);

	sma1305->spk_rcv_mode = SMA1305_SPEAKER_MODE;

	sma1305->sdo_bypass_flag = false;
	sma1305->force_amp_power_down = false;
	sma1305->amp_power_status = false;
	sma1305->pll_matches = sma1305_pll_matches;
	sma1305->num_of_pll_matches =
		ARRAY_SIZE(sma1305_pll_matches);
	sma1305->irq = -1;

	if (gpio_is_valid(sma1305->gpio_int)) {

		dev_info(&client->dev, "%s , i2c client name: %s\n",
			__func__, dev_name(sma1305->dev));

		ret = gpio_request(sma1305->gpio_int, "sma1305-irq");
		if (ret) {
			dev_info(&client->dev, "gpio_request failed\n");
			return ret;
		}

		sma1305->irq = gpio_to_irq(sma1305->gpio_int);

		/* Get SMA1305 IRQ */
		if (sma1305->irq < 0) {
			dev_warn(&client->dev, "interrupt disabled\n");
		} else {
		/* Request system IRQ for SMA1305 */
			ret = request_threaded_irq(sma1305->irq,
				NULL, sma1305_isr, IRQF_ONESHOT | IRQF_SHARED |
				IRQF_TRIGGER_LOW, "sma1305", sma1305);
			if (ret < 0) {
				dev_err(&client->dev, "failed to request IRQ(%u) [%d]\n",
						sma1305->irq, ret);
				sma1305->irq = -1;
				i2c_set_clientdata(client, NULL);
				devm_free_irq(&client->dev,
						sma1305->irq, sma1305);
				devm_kfree(&client->dev, sma1305);
				return ret;
			}
			disable_irq((unsigned int)sma1305->irq);
		}
	} else {
		dev_err(&client->dev,
			"interrupt signal input pin is not found\n");
	}

	atomic_set(&sma1305->irq_enabled, false);
	i2c_set_clientdata(client, sma1305);

	ret = devm_snd_soc_register_component(&client->dev,
			&sma1305_component, sma1305_dai, 1);

	if (ret) {
		dev_err(&client->dev, "Failed to register component");
		snd_soc_unregister_component(&client->dev);

		if (sma1305)
			devm_kfree(&client->dev, sma1305);

		return ret;
	}

	/* Create sma1305 sysfs attributes */
	sma1305->attr_grp = &sma1305_attr_group;
	ret = sysfs_create_group(sma1305->kobj, sma1305->attr_grp);

	if (ret) {
		dev_err(&client->dev,
			"failed to create attribute group [%d]\n", ret);
		sma1305->attr_grp = NULL;
	}

	return ret;
}

static int sma1305_i2c_remove(struct i2c_client *client)
{
	struct sma1305_priv *sma1305 =
		(struct sma1305_priv *) i2c_get_clientdata(client);

	dev_info(&client->dev, "%s\n", __func__);

	if (sma1305 != NULL) {
		cancel_delayed_work_sync(&sma1305->check_fault_work);
		snd_soc_unregister_component(&client->dev);

		if (sma1305->irq < 0)
			devm_free_irq(&client->dev, sma1305->irq, sma1305);

		devm_kfree(&client->dev, sma1305);
	}

	return 0;
}

static const struct i2c_device_id sma1305_i2c_id[] = {
	{"sma1305", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sma1305_i2c_id);

static const struct of_device_id sma1305_of_match[] = {
	{ .compatible = "siliconmitus,sma1305", },
	{ }
};
MODULE_DEVICE_TABLE(of, sma1305_of_match);

static struct i2c_driver sma1305_i2c_driver = {
	.driver = {
		.name = "sma1305",
		.of_match_table = sma1305_of_match,
	},
	.probe = sma1305_i2c_probe,
	.remove = sma1305_i2c_remove,
	.id_table = sma1305_i2c_id,
};

static int __init sma1305_init(void)
{
	int ret;

	ret = i2c_add_driver(&sma1305_i2c_driver);

	if (ret)
		pr_err("Failed to register sma1305 I2C driver: %d\n", ret);

	return ret;
}

static void __exit sma1305_exit(void)
{
	i2c_del_driver(&sma1305_i2c_driver);
}

module_init(sma1305_init);
module_exit(sma1305_exit);

MODULE_DESCRIPTION("ALSA SoC SMA1305 driver");
MODULE_AUTHOR("Justin, <jeongjin.kim@siliconmitus.com>");
MODULE_AUTHOR("KS Jo, <kiseok.jo@irondevice.com>");
MODULE_LICENSE("GPL v2");
