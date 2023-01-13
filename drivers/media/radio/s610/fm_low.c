/*
 * drivers/media/radio/s610/fm_low.c
 *
 * FM Radio Rx Low level driver for SAMSUNG S610 chip
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#if defined(CONFIG_RADIO_S610)
#include "s61x_io_map.h"
#elif defined(CONFIG_RADIO_S621)
#include "s621_io_map.h"
#endif /* defined(CONFIG_RADIO_S610) */
#include "fm_low_struc.h"
#include "radio-s610.h"
#include "fm_low_ref.h"

/* Numeric identifier embedded in the code. */
const u32 build_identifier_integer = 0x20105555;
void (*handler_if_count)(struct s610_radio *radio) = NULL;
void (*handler_audio_pause)(struct s610_radio *radio) = NULL;

#if defined(CONFIG_RADIO_S621)
void print_ana_register(void)
{
		dev_info(gradio->dev, "%s", "========================================================");
		dev_info(gradio->dev, "%s", " Ana Registers  ");
		dev_info(gradio->dev, "%s", "========================================================");
		dev_info(gradio->dev, "AUX_CON    : 0x%xh", fmspeedy_get_reg_work(AUX_CON));
		dev_info(gradio->dev, "BPLL_CON1  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON1));
		dev_info(gradio->dev, "BPLL_CON2  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON2));
		dev_info(gradio->dev, "BPLL_CON3  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON3));
		dev_info(gradio->dev, "BPLL_CON4  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON4));
		dev_info(gradio->dev, "BPLL_CON5  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON5));
		dev_info(gradio->dev, "BPLL_CON7  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON7));
		dev_info(gradio->dev, "BPLL_CON8  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON8));
		dev_info(gradio->dev, "BPLL_CON9  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON9));
		dev_info(gradio->dev, "PLL_CON1   : 0x%xh", fmspeedy_get_reg_work(PLL_CON1));
		dev_info(gradio->dev, "PLL_CON2   : 0x%xh", fmspeedy_get_reg_work(PLL_CON2));
		dev_info(gradio->dev, "PLL_CON3   : 0x%xh", fmspeedy_get_reg_work(PLL_CON3));
		dev_info(gradio->dev, "PLL_CON4   : 0x%xh", fmspeedy_get_reg_work(PLL_CON4));
		dev_info(gradio->dev, "PLL_CON5   : 0x%xh", fmspeedy_get_reg_work(PLL_CON5));
		dev_info(gradio->dev, "PLL_CON6   : 0x%xh", fmspeedy_get_reg_work(PLL_CON6));
		dev_info(gradio->dev, "PLL_CON7   : 0x%xh", fmspeedy_get_reg_work(PLL_CON7));
		dev_info(gradio->dev, "PLL_CON8   : 0x%xh", fmspeedy_get_reg_work(PLL_CON8));
		dev_info(gradio->dev, "PLL_CON9   : 0x%xh", fmspeedy_get_reg_work(PLL_CON9));
		dev_info(gradio->dev, "PLL_CON10  : 0x%xh", fmspeedy_get_reg_work(PLL_CON10));
		dev_info(gradio->dev, "PLL_CON12  : 0x%xh", fmspeedy_get_reg_work(PLL_CON12));
		dev_info(gradio->dev, "PLL_CON13  : 0x%xh", fmspeedy_get_reg_work(PLL_CON13));
		dev_info(gradio->dev, "PLL_CON14  : 0x%xh", fmspeedy_get_reg_work(PLL_CON14));
		dev_info(gradio->dev, "PLL_CON15  : 0x%xh", fmspeedy_get_reg_work(PLL_CON15));
		dev_info(gradio->dev, "PLL_CON16  : 0x%xh", fmspeedy_get_reg_work(PLL_CON16));
		dev_info(gradio->dev, "PLL_CON17  : 0x%xh", fmspeedy_get_reg_work(PLL_CON17));
		dev_info(gradio->dev, "PLL_CON18  : 0x%xh", fmspeedy_get_reg_work(PLL_CON18));
		dev_info(gradio->dev, "PLL_CON19  : 0x%xh", fmspeedy_get_reg_work(PLL_CON19));
		dev_info(gradio->dev, "PLL_CON20  : 0x%xh", fmspeedy_get_reg_work(PLL_CON20));
		dev_info(gradio->dev, "ADC_CON1   : 0x%xh", fmspeedy_get_reg_work(ADC_CON1));
		dev_info(gradio->dev, "ADC_CON2   : 0x%xh", fmspeedy_get_reg_work(ADC_CON2));
		dev_info(gradio->dev, "RX_RF_CON1 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON1));
		dev_info(gradio->dev, "RX_RF_CON2 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON2));
		dev_info(gradio->dev, "RX_RF_CON3 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON3));
		dev_info(gradio->dev, "RX_RF_CON4 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON4));
		dev_info(gradio->dev, "ADC_CON0   : 0x%xh", fmspeedy_get_reg_work(ADC_CON0));
		dev_info(gradio->dev, "BPLL_CON6  : 0x%xh", fmspeedy_get_reg_work(BPLL_CON6));
		dev_info(gradio->dev, "PLL_CON11  : 0x%xh", fmspeedy_get_reg_work(PLL_CON11));
		dev_info(gradio->dev, "RX_RF_CON0 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON0));
}
#elif defined(CONFIG_RADIO_S610)
void print_ana_register(void)
{
		dev_info(gradio->dev, "%s", "========================================================");
		dev_info(gradio->dev, "%s", " Ana Registers  ");
		dev_info(gradio->dev, "%s", "========================================================");
		dev_info(gradio->dev, "PLL_CON1   : 0x%xh", fmspeedy_get_reg_work(PLL_CON1));
		dev_info(gradio->dev, "PLL_CON2   : 0x%xh", fmspeedy_get_reg_work(PLL_CON2));
		dev_info(gradio->dev, "PLL_CON3   : 0x%xh", fmspeedy_get_reg_work(PLL_CON3));
		dev_info(gradio->dev, "PLL_CON4   : 0x%xh", fmspeedy_get_reg_work(PLL_CON4));
		dev_info(gradio->dev, "PLL_CON5   : 0x%xh", fmspeedy_get_reg_work(PLL_CON5));
		dev_info(gradio->dev, "PLL_CON6   : 0x%xh", fmspeedy_get_reg_work(PLL_CON6));
		dev_info(gradio->dev, "PLL_CON7   : 0x%xh", fmspeedy_get_reg_work(PLL_CON7));
		dev_info(gradio->dev, "PLL_CON8   : 0x%xh", fmspeedy_get_reg_work(PLL_CON8));
		dev_info(gradio->dev, "PLL_CON9   : 0x%xh", fmspeedy_get_reg_work(PLL_CON9));
		dev_info(gradio->dev, "PLL_CON10  : 0x%xh", fmspeedy_get_reg_work(PLL_CON10));
		dev_info(gradio->dev, "PLL_CON11  : 0x%xh", fmspeedy_get_reg_work(PLL_CON11));
		dev_info(gradio->dev, "PLL_CON12  : 0x%xh", fmspeedy_get_reg_work(PLL_CON12));
		dev_info(gradio->dev, "PLL_CON13  : 0x%xh", fmspeedy_get_reg_work(PLL_CON13));
		dev_info(gradio->dev, "PLL_CON14  : 0x%xh", fmspeedy_get_reg_work(PLL_CON14));
		dev_info(gradio->dev, "PLL_CON15  : 0x%xh", fmspeedy_get_reg_work(PLL_CON15));
		dev_info(gradio->dev, "PLL_CON16  : 0x%xh", fmspeedy_get_reg_work(PLL_CON16));
		dev_info(gradio->dev, "PLL_CON17  : 0x%xh", fmspeedy_get_reg_work(PLL_CON17));
		dev_info(gradio->dev, "PLL_CON18  : 0x%xh", fmspeedy_get_reg_work(PLL_CON18));
		dev_info(gradio->dev, "PLL_CON19  : 0x%xh", fmspeedy_get_reg_work(PLL_CON19));
		dev_info(gradio->dev, "PLL_CON20  : 0x%xh", fmspeedy_get_reg_work(PLL_CON20));
		dev_info(gradio->dev, "PLL_CON21  : 0x%xh", fmspeedy_get_reg_work(PLL_CON21));
		dev_info(gradio->dev, "AUX_CON0   : 0x%xh", fmspeedy_get_reg_work(AUX_CON0));
		dev_info(gradio->dev, "AUX_CON1   : 0x%xh", fmspeedy_get_reg_work(AUX_CON1));
		dev_info(gradio->dev, "AUX_CON2   : 0x%xh", fmspeedy_get_reg_work(AUX_CON2));
		dev_info(gradio->dev, "AUX_CON3   : 0x%xh", fmspeedy_get_reg_work(AUX_CON3));
		dev_info(gradio->dev, "AUX_CON4   : 0x%xh", fmspeedy_get_reg_work(AUX_CON4));
		dev_info(gradio->dev, "AUX_CON5   : 0x%xh", fmspeedy_get_reg_work(AUX_CON5));
		dev_info(gradio->dev, "AUX_CON6   : 0x%xh", fmspeedy_get_reg_work(AUX_CON6));
		dev_info(gradio->dev, "ADC_CON0   : 0x%xh", fmspeedy_get_reg_work(ADC_CON0));
		dev_info(gradio->dev, "ADC_CON1   : 0x%xh", fmspeedy_get_reg_work(ADC_CON1));
		dev_info(gradio->dev, "ADC_CON2   : 0x%xh", fmspeedy_get_reg_work(ADC_CON2));
		dev_info(gradio->dev, "RX_RF_CON0 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON1));
		dev_info(gradio->dev, "RX_RF_CON1 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON1));
		dev_info(gradio->dev, "RX_RF_CON2 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON2));
		dev_info(gradio->dev, "RX_RF_CON3 : 0x%xh", fmspeedy_get_reg_work(RX_RF_CON3));
}
#endif /* defined(CONFIG_RADIO_S621) */

/****************************************************************************/
/* Functions for initialization */
/****************************************************************************/

int fm_boot(struct s610_radio *radio)
{
	bool aux_ret;

	fm_audio_control(radio, 0, 0, 0, 0);

#if defined(CONFIG_RADIO_S621)
	/* power on for FM digital block */
	fm_pwron();
#endif /* defined(CONFIG_RADIO_S621) */
	aux_ret = fm_aux_pll_initialize();
	if (!aux_ret)
		return -1;
#if defined(CONFIG_RADIO_S610)
	/* power on for FM digital block */
	fm_pwron();
#endif /* defined(CONFIG_RADIO_S610) */
	fm_lo_initialize(radio);

	fm_initialize(radio);

	return 0;
}

void fm_power_off(void)
{
	fm_iclkaux_set(0); /* restore CLKMUX */

	fm_lo_off();

	/* power off for FM digital block */
	fm_pwroff();

	fm_aux_pll_off();

	/* free RDS buffer */
	kfree(gradio->low->rds_buffer_mem);
}

void fm_iclkaux_set(u32 data)
{
#if defined(CONFIG_RADIO_S610)
	fmspeedy_set_reg_field(CLK_SEL, 0, (0x0001<<0), data); /* iCLKAux */
#elif defined(CONFIG_RADIO_S621)
	fmspeedy_set_reg_field(CLK_SEL, 2, (0x0001<<2), data);
#endif /* defined(CONFIG_RADIO_S610) */
	dev_info(gradio->dev, "%s: iClk Aux: 0x%xh get val: 0x%xh", __func__,
		data,
		fmspeedy_get_reg(CLK_SEL));
}

void fm_initialize(struct s610_radio *radio)
{
	API_ENTRY(radio);

	/* Initialize the analogue block */
	fm_rx_init();

	fm_iclkaux_set(radio->iclkaux);

	/* Set the demod reg. */
#if defined(CONFIG_RADIO_S610)
	radio->low->fm_config.demod_conf_ini |= 0x2C000;
#elif defined(CONFIG_RADIO_S621)
	radio->low->fm_config.demod_conf_ini |= 0x22C000;
#endif /* defined(CONFIG_RADIO_S610) */
	dev_info(radio->dev, "%s():demod_conf_ini[%08X]",
		__func__,
		radio->low->fm_config.demod_conf_ini);

	fmspeedy_set_reg(FM_DEMOD_CONFIG, radio->low->fm_config.demod_conf_ini);

	if (radio->vol_3db_att)
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 10, (0x01 << 10), 1);
	else
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 10, (0x01 << 10), 0);

	fmspeedy_set_reg(FM_DEMOD_NARROW_THRESHOLDS, radio->low->fm_config.narrow_thres_ini);
	fmspeedy_set_reg(FM_DEMOD_SNR_ADJUST, radio->low->fm_config.snr_adj_ini);
	fmspeedy_set_reg(FM_DEMOD_STEREO_THRESHOLD, radio->low->fm_config.stereo_thres_ini);
	fmspeedy_set_reg(FM_DEMOD_SNR_SMOOTH_CONFIG, radio->low->fm_config.snr_smooth_conf_ini);
	fmspeedy_set_reg(FM_DEMOD_SOFT_MUFFLE_COEFFS,
		radio->low->fm_config.soft_muffle_conf_ini.muffle_coeffs);

	fmspeedy_set_reg_field(FM_DEMOD_CONFIG2, 3, (0x0007 << 3),
		radio->low->fm_config.soft_mute_atten_max_ini);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG2, 0, 0x0007,
		radio->low->fm_config.soft_muffle_conf_ini.lpf_bw);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG2, 6, (0x0001 << 6),
		radio->low->fm_config.soft_muffle_conf_ini.lpf_en);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG2, 7, (0x0001 << 7),
		radio->low->fm_config.soft_muffle_conf_ini.lpf_auto);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG2, 8, (0x0001 << 8), 1);

	if (!radio->without_elna)
		radio->rssi_adjust = RSSI_ADJUST_WITHOUT_ELNA_VALUE;

	fmspeedy_set_reg(FM_DEMOD_RSSI_ADJUST, radio->low->fm_config.rssi_adj_ini+radio->rssi_adjust);
	APIEBUG(radio, "%s(): FM_DEMOD_RSSI_ADJUST:0x%x %d %d", __func__,
		fmspeedy_get_reg(FM_DEMOD_RSSI_ADJUST), radio->low->fm_config.rssi_adj_ini, radio->rssi_adjust);

	fmspeedy_set_reg(FM_AGC_THRESH_ADC, 0xFF64);

#ifdef USE_IQ_IMBAL_SMOOTH
#if defined(CONFIG_RADIO_S610)
	fmspeedy_set_reg(FM_DEMOD_IMAGE_TRIM_SMOOTH_CONFIG, 0x081C);
#elif defined(CONFIG_RADIO_S621)
	fmspeedy_set_reg(FM_DEMOD_IMAGE_TRIM_SMOOTH_CONFIG, 0x09C);
#endif /* defined(CONFIG_RADIO_S610) */
#endif /*USE_IQ_IMBAL_SMOOTH*/

#ifdef USE_SPUR_CANCEL
	if (radio->tc_on)
		fmspeedy_set_reg(FM_DEMOD_TONEREJ_THRESH_ADAPT, 0x18);
#endif

#if defined(CONFIG_SOC_EXYNOS3830) && defined(CONFIG_RADIO_S610)
	/* Enable the volume control */
	fmspeedy_set_reg_field(PLL_CON18, 11, (0x0001 << 11), 1);
#elif defined(CONFIG_RADIO_S621)
	/* Enable the volume control */
	fmspeedy_set_reg_field(FM_VOLUME_CTRL, 13, (0x0001 << 13), 1);
#else
	/* Enable the volume control */
	fmspeedy_set_reg_field(PLL_CON18, 11, (0x0001 << 11), 1);
#endif /* defined(CONFIG_SOC_EXYNOS3830) && defined(CONFIG_RADIO_S610) */
	fm_set_band(radio, 0); /*FM band(87.5 ~ 108 MHz)*/
	fm_set_freq_step(radio, 1); /*freq_step(100 KHz)*/

	fm_set_blend_mute(radio);
	fm_set_mute(TRUE);
#ifdef	USE_RINGBUFF_API
	/* Create the RDS buffer. */
	if (radio->rds_parser_enable)
		radio->low->rds_buffer_mem = kzalloc(FM_RDS_MEM_SIZE_PARSER, GFP_KERNEL);
	else
		radio->low->rds_buffer_mem = kzalloc(FM_RDS_MEM_SIZE, GFP_KERNEL);

	/* ringbuf init */
	if (radio->rds_parser_enable)
		radio->rds_rb.size = FM_RDS_MEM_SIZE_PARSER;
	else
		radio->rds_rb.size = FM_RDS_MEM_SIZE;

	radio->rds_rb.buf = radio->low->rds_buffer_mem;
	radio->rds_rb.head = radio->rds_rb.tail = radio->rds_rb.buf;
#else	/* USE_RINGBUFF_API */
	/* Create the RDS buffer. */
	radio->low->rds_buffer = kzalloc(sizeof(struct rds_buf_conf),
			GFP_KERNEL);
	radio->low->rds_buffer_mem = kzalloc(FM_RDS_MEM_SIZE, GFP_KERNEL);

	radio->low->rds_buffer->base = radio->low->rds_buffer_mem;
	radio->low->rds_buffer->index = radio->low->rds_buffer->outdex = 0;
	radio->low->rds_buffer->size = FM_RDS_MEM_SIZE;
#endif	/* USE_RINGBUFF_API */

	fm_rds_flush_buffers(radio, FALSE);

	API_EXIT(radio);
}

/****************************************************************************/
/* Functions for conversion */
/****************************************************************************/

u16 if_count_device_to_host(struct s610_radio *radio, u16 val)
{
	bool negative = !!(val & 0x8000);
	u32 resp;

	if (negative)
		val = -val;

	resp = ((u32) val) / 128;

	if (resp > 0x7FFF)
		resp = 0x7FFF;

	return negative ? (u16) -resp : (u16) resp;
}

#define AGGR_RSSI_OFFSET (-114)

static u16 aggr_rssi_host_to_device(u8 val)
{
	s8 val_t = (val > 127) ? ((s16)(val & 0x00FF) - 256) : (s16) val;
	u16 resp;

	if (val_t >= AGGR_RSSI_OFFSET)
		resp = ((u16) val_t - AGGR_RSSI_OFFSET) * 4;
	else
		resp = 0;

	return resp;
}

u8 aggr_rssi_device_to_host(u16 val)
{
	s8 resp;

	resp = (val / 4) + AGGR_RSSI_OFFSET;
	return ((u8) resp) & 0x00FF;
}

u16 rssi_device_to_host(u16 digi_rssi, u16 agc_gain, u16 rssi_adj)
{
	u16 aggr_rssi;
	u16 digi_rssi_t = (digi_rssi & 0x1FF);
	u16 digi_gain = (agc_gain & 0xF000) >> 12;
	u16 ana_gain = (agc_gain & 0x0F80) >> 7;

	aggr_rssi = digi_rssi_t - (12 * digi_gain) - (8 * ana_gain) - rssi_adj
		+ 160 + 84;

	return aggr_rssi_device_to_host(aggr_rssi);
}

/****************************************************************************/
/*Functions for the interaction with a device*/
/****************************************************************************/
void fm_set_audio_gain(struct s610_radio *radio, u16 gain)
{
	if (gain >= radio->vol_num)
		gain = (radio->vol_num <= 1) ? 0 : radio->vol_num - 1;

#if defined(CONFIG_RADIO_S610)
	fmspeedy_set_reg_field(PLL_CON18, 0, (0x07FF << 0),
		radio->vol_level_mod[gain]);
#elif defined(CONFIG_RADIO_S621)
	fmspeedy_set_reg_field(FM_VOLUME_CTRL, 0, 0x01FFF,
		radio->vol_level_mod[gain]);
#endif /* defined(CONFIG_RADIO_S610) */
}

static bool is_freq_in_spur(int freq, u32 *freq_array, int max_freq)
{
	int i;

	for (i = 0; i < max_freq; i++) {
		if (freq_array[i] == freq)
			return TRUE;
	}
	return FALSE;
}

#if defined(CONFIG_RADIO_S610)
#define AGC_CONFIG_WBRSSI_DISABLE	0x10D1
#define AGC_CONFIG_WBRSSI_ENABLE	0x105F
#elif defined(CONFIG_RADIO_S621)
#define AGC_CONFIG_WBRSSI_DISABLE	0x20D1
#define AGC_CONFIG_WBRSSI_ENABLE	0x205F
#endif /* defined(CONFIG_RADIO_S610) */

void enable_agc_config_wbrssi(struct s610_radio *radio, bool onoff)
{
#if defined(CONFIG_RADIO_S610)
	if (onoff) {
		/* AGC config WBRSSI_LO/HI enable) */
		fmspeedy_set_reg(FM_AGC_CONFIG, AGC_CONFIG_WBRSSI_ENABLE);
		fmspeedy_set_reg(FM_AGC_THRESH_RSSI, 0x0804);
		radio->agc_enable = AGC_CONFIG_WBRSSI_ENABLE;
	} else {
		/* AGC config WBRSSI_LO/HI disable */
		fmspeedy_set_reg(FM_AGC_CONFIG, AGC_CONFIG_WBRSSI_DISABLE);
		fmspeedy_set_reg(FM_AGC_THRESH_RSSI, 0x0906);
		radio->agc_enable = AGC_CONFIG_WBRSSI_DISABLE;
	}
#elif defined(CONFIG_RADIO_S621)
	if (onoff) {
		/* AGC config WBRSSI_LO/HI enable) */
		fmspeedy_set_reg(FM_AGC_CONFIG, AGC_CONFIG_WBRSSI_ENABLE);
		fmspeedy_set_reg(FM_AGC_THRESH_RSSI, 0x84);
		radio->agc_enable = AGC_CONFIG_WBRSSI_ENABLE;
	} else {
		/* AGC config WBRSSI_LO/HI disable */
		fmspeedy_set_reg(FM_AGC_CONFIG, AGC_CONFIG_WBRSSI_DISABLE);
		fmspeedy_set_reg(FM_AGC_THRESH_RSSI, 0x96);
		radio->agc_enable = AGC_CONFIG_WBRSSI_DISABLE;
	}
#endif /* defined(CONFIG_RADIO_S610) */

	dev_info(radio->dev, "%s(%d):FM_AGC_CONFIG:%08X, FM_AGC_THRESH_RSSI:%08X ",
		__func__, onoff, fmspeedy_get_reg(FM_AGC_CONFIG),
		fmspeedy_get_reg(FM_AGC_THRESH_RSSI));
}

void reset_agc_gain(void)
{
	/* FM AGC gain reset */
	fmspeedy_set_reg(FM_AGC_GAIN_FORCE, 0x33C);
	msleep(5);
	fmspeedy_set_reg(FM_AGC_GAIN_FORCE, 0x13C);
}

#if defined(CONFIG_RADIO_S610)
#define NEW_TRF_ENABLE (0x1)
#define NEW_TRF_ALWAYS_ADAPT (0x1 << 1)
int set_new_trj_ref_angle(struct s610_radio *radio, int tune_freq, int spur_freq[], int num_spur)
{
	int i;
	int result = 0;
	int spur_freq_sel;
	int diff = 999999;
	int angle_first, angle_last;

	for (i = 0; i < num_spur; i++) {
		int quotient;
		int small, big;
		int spur_harmonic;
		int temp_diff;

		quotient = tune_freq / spur_freq[i];
		small = (spur_freq[i] * quotient);
		big = (spur_freq[i] * (quotient + 1));

		spur_harmonic = ((tune_freq - small) < (big - tune_freq)) ? small : big;
		temp_diff = spur_harmonic - tune_freq;

		if (ABS(diff) > ABS(temp_diff)) {
			spur_freq_sel = spur_freq[i];
			diff = temp_diff;
		}
	}

	if (((-100 <= diff) && (diff <= 100)) && (ABS(diff) != (spur_freq_sel / 2))) {
		dev_info(radio->dev, "[%06d] new TRF enable for %dkHz harmonic spur",
			radio->low->fm_state.freq, spur_freq_sel);
		result = NEW_TRF_ENABLE;
		if (diff != 0) {
			angle_first = ((diff * 1000000000000000) / 298023224) & 0x3fffffff;
			angle_last = ((-diff * 1000000000000000) / 298023224) & 0x3fffffff;

			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_FIRST, angle_first);
			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_LAST, angle_last);

			result |= NEW_TRF_ALWAYS_ADAPT;
		} else
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
	}

	return result;
}

void fm_set_freq(struct s610_radio *radio, u32 freq, bool mix_hi)
{
	int ii;
	u32 fifo_tmp;

	API_ENTRY(radio);
	APIEBUG(radio, "set freq: %d", radio->low->fm_state.freq);

	radio->low->fm_tune_info.rx_setup.fm_freq_khz = freq;
	radio->low->fm_tune_info.rx_setup.fm_freq_hz = freq * 1000;

	if (radio->dual_clk_on
			&& is_freq_in_spur(radio->low->fm_state.freq, fm_dual_clk_init, radio->dual_clk_on)) {
		if (mix_hi) {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz + 225220;
			radio->low->fm_tune_info.rx_setup.demod_if = 0xF85;
		} else {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz - 225220;
			radio->low->fm_tune_info.rx_setup.demod_if = 0x7B;
		}
	} else {
		if (mix_hi) {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz + 224609;
			radio->low->fm_tune_info.rx_setup.demod_if = 0xF8D;
		} else {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz - 224609;
			radio->low->fm_tune_info.rx_setup.demod_if = 0x73;
		}
	}

	fm_lo_prepare_setup(radio);

#if defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630)
	if (freq <= 90000)
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x20;
	else
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x12;

	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x6);
#elif defined(CONFIG_SOC_EXYNOS7885) || defined(CONFIG_SOC_EXYNOS3830)
	if (freq <= 70000)
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x28;
	else if (freq < 80000)
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x16;
	else if (freq < 90000)
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x12;
	else if (freq < 100000)
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x0A;
	else
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x05;

	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
#endif /* defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS3830) */
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 1);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001<<4), 0);
	fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_FIRST, 0x0);
	fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_LAST, 0x0);

	if (!radio->without_elna)
		radio->rssi_adjust = RSSI_ADJUST_WITHOUT_ELNA_VALUE;

	fmspeedy_set_reg(FM_DEMOD_RSSI_ADJUST, radio->low->fm_config.rssi_adj_ini+radio->rssi_adjust);
	APIEBUG(radio, "%s(): FM_DEMOD_RSSI_ADJUST: 0x%x %d %d", __func__,
			fmspeedy_get_reg(FM_DEMOD_RSSI_ADJUST), radio->low->fm_config.rssi_adj_ini, radio->rssi_adjust);
#ifdef	IDLE_POLLING_ENABLE
	fm_idle_periodic_cancel((unsigned long) radio);
#endif	/*IDLE_POLLING_ENABLE*/

	if (radio->trf_on) {
		if (is_freq_in_spur(radio->low->fm_state.freq, fm_spur_trf_init,
				radio->trf_on)) {
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 1);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001<<4), 1);
			if (radio->seek_status == FM_TUNER_PRESET_MODE)
				udelay(100);
			dev_info(radio->dev, "TRF ON [%06d][FM_DEMOD_CONFIG : 0x%08X]",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_CONFIG));
		}
	}

#ifdef	IDLE_POLLING_ENABLE
	fm_idle_periodic_update((unsigned long) radio);
#endif	/*IDLE_POLLING_ENABLE*/

#if defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630)
	if (radio->without_elna) {
		if (!is_freq_in_spur(radio->low->fm_state.freq, fm_spur_trf_init, radio->trf_on) &&
			!is_freq_in_spur(radio->low->fm_state.freq, fm_dual_clk_init, radio->dual_clk_on)) {
			int ret;
			int spur_freq[2] = {320, 768};

			ret = set_new_trj_ref_angle(radio, radio->low->fm_state.freq,
				spur_freq, sizeof(spur_freq) / sizeof(int));
			if (NEW_TRF_ENABLE & ret) {
				dev_info(radio->dev, "[%06d] new TRF On", radio->low->fm_state.freq);
				fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
				fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
				if (NEW_TRF_ALWAYS_ADAPT & ret)
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
			}
		}
	}
#elif defined(CONFIG_SOC_EXYNOS7885) || defined(CONFIG_SOC_EXYNOS3830)
	{	/* TRF */
#define NUM_CUSTOM_TRF	17
		bool is_custom_trf;
		u32 custom_trf[NUM_CUSTOM_TRF][4] = {
			/* freq, first angle, last angle, bandwidth */
			{77600, 0x0, 0x0, 0x2},
			{77700, 0x3FAE146B, 0x0051EB84, 0x6},
			{77900, 0x13FFFFFF, 0x2C000001, 0x2},
			{78000, 0x0, 0x0, 0x2},
			{78100, 0x2C000001, 0x13FFFFFF, 0x2},
			{78400, 0x0, 0x0, 0x2},
			{79700, 0x3FAE146B, 0x0051EB84, 0x8},
			{79900, 0x13FFFFFF, 0x2C000001, 0x8},
			{80000, 0x0, 0x0, 0x2},
			{82100, 0x3C000001, 0x03FFFFFF, 0x2},
			{84000, 0x0, 0x0, 0x4},
			{84700, 0x34000000, 0x0C000000, 0x2},
			{85300, 0x3C000001, 0x03FFFFFF, 0x2},
			{86200, 0x07FFFFFF, 0x38000001, 0x2},
			{86400, 0x34000000, 0x0C000000, 0x2},
			{86600, 0x38000001, 0x07FFFFFF, 0x2},
			{108000, 0x0, 0x0, 0x2},
		};

		is_custom_trf = false;
		for (ii = 0; ii < NUM_CUSTOM_TRF; ii++) {
			if (custom_trf[ii][0] == freq) {
				fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), custom_trf[ii][3]);
			    fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
			    fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
			    fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
				fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_FIRST, custom_trf[ii][1]);
				fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_LAST, custom_trf[ii][2]);

				dev_info(radio->dev, "[%06d] new TRF(custom) On", radio->low->fm_state.freq);
				dev_info(radio->dev, "TRF [%06d][FM_NEW_TRJ_REF_ANGLE_FIRST : 0x%08X]",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_NEW_TRJ_REF_ANGLE_FIRST));
				dev_info(radio->dev, "TRF [%06d][FM_NEW_TRJ_REF_ANGLE_LAST : 0x%08X]",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_NEW_TRJ_REF_ANGLE_LAST));
				dev_info(radio->dev, "TRF [%06d][FM_DEMOD_CONFIG : 0x%08X]",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_CONFIG));

				is_custom_trf = true;

				break;
			}
		}

		if (!is_custom_trf) {
			if (((radio->low->fm_state.freq != 100000) && (radio->low->fm_state.freq != 104000)) &&
				!is_freq_in_spur(radio->low->fm_state.freq, fm_dual_clk_init, radio->dual_clk_on)) {

				int ret;
				int spur_freq[2] = {320, 768};

				ret = set_new_trj_ref_angle(radio, radio->low->fm_state.freq,
					spur_freq, sizeof(spur_freq) / sizeof(int));
				if (NEW_TRF_ENABLE & ret) {
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
					if (NEW_TRF_ALWAYS_ADAPT & ret)
						fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);

					dev_info(radio->dev, "TRF [%06d][FM_NEW_TRJ_REF_ANGLE_FIRST : 0x%08X]",
						radio->low->fm_state.freq,
						fmspeedy_get_reg(FM_NEW_TRJ_REF_ANGLE_FIRST));
					dev_info(radio->dev, "TRF [%06d][FM_NEW_TRJ_REF_ANGLE_LAST : 0x%08X]",
						radio->low->fm_state.freq,
						fmspeedy_get_reg(FM_NEW_TRJ_REF_ANGLE_LAST));
					dev_info(radio->dev, "TRF [%06d][FM_DEMOD_CONFIG : 0x%08X]",
						radio->low->fm_state.freq,
						fmspeedy_get_reg(FM_DEMOD_CONFIG));
					dev_info(radio->dev, "[%06d][FM_DEMOD_TONEREJ_THRESH_ADAPT : 0x%08X]",
						radio->low->fm_state.freq,
						fmspeedy_get_reg(FM_DEMOD_TONEREJ_THRESH_ADAPT));
				}
			} else {
				dev_info(radio->dev, "Skip TRF ON");
			}
		}
	}
#elif 0 /*defined(CONFIG_SOC_EXYNOS3830)*/
	if (radio->without_elna) {
		if (freq == 108000) {
			dev_info(radio->dev, "[%06d] new TRF On", radio->low->fm_state.freq);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
		} else if (freq == 88000) {
			dev_info(radio->dev, "[%06d] new TRF On", radio->low->fm_state.freq);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_FIRST, 0x10000000);
			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_LAST, 0x30000000);
		} else if (freq == 88100) {
			dev_info(radio->dev, "[%06d] new TRF On", radio->low->fm_state.freq);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_FIRST, 0x3C000000);
			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_LAST, 0x04000000);
		} else if (freq == 96000) {
			dev_info(radio->dev, "[%06d] new TRF On", radio->low->fm_state.freq);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_FIRST, 0x2C000000);
			fmspeedy_set_reg(FM_NEW_TRJ_REF_ANGLE_LAST, 0x14000000);
		} else {
			if (!is_freq_in_spur(radio->low->fm_state.freq, fm_spur_trf_init, radio->trf_on) &&
				!is_freq_in_spur(radio->low->fm_state.freq, fm_dual_clk_init, radio->dual_clk_on)) {
				int ret;
				int spur_freq[2] = {320, 512};

				ret = set_new_trj_ref_angle(radio, radio->low->fm_state.freq,
					spur_freq, sizeof(spur_freq) / sizeof(int));
				if (NEW_TRF_ENABLE & ret) {
					dev_info(radio->dev, "[%06d] new TRF On", radio->low->fm_state.freq);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
					if (NEW_TRF_ALWAYS_ADAPT & ret)
						fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
				}
			}
		}
	}
#endif /* defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630) */

#ifdef USE_SPUR_CANCEL
	if (radio->tc_on)
		fm_rx_check_spur(radio);
#endif

#ifdef USE_IQ_IMBAL_SMOOTH
	/* Clear the smooth config lock for IQ imbalance */
	fmspeedy_set_reg_field(FM_DEMOD_IMAGE_TRIM_SMOOTH_CONFIG, 10, (0x0001 << 10), 0);
#endif /*USE_IQ_IMBAL_SMOOTH*/

	/* Set up CDAC */
	fmspeedy_set_reg_field(RX_RF_CON2, 22, (0x003F << 22),
			radio->low->fm_tune_info.rx_setup.lna_cdac);

	if (freq == 104000) {
		fmspeedy_set_reg(PLL_CON5, 0x0A20D0);
		fmspeedy_set_reg_field(RX_RF_CON3, 17, (0x0001<<17), 1); /* AUX_SEL_RX_ADC_CLK_30M  */
		fmspeedy_set_reg_field(PLL_CON2, 9, (0x0001<<9), 0); /* LO_CLKREF_ADC_SEL_CLKREF */
		fmspeedy_set_reg_field(AUX_CON3, 27, (0x0001<<27), 0); /* XTAL_EN_FM_BUF off*/
	} else {
		fmspeedy_set_reg(PLL_CON5, 0x24A0D0);
		fmspeedy_set_reg_field(RX_RF_CON3, 17, (0x0001<<17), 0); /* AUX_SEL_RX_ADC_CLK_30M  */
		fmspeedy_set_reg_field(PLL_CON2, 9, (0x0001<<9), 1); /* LO_CLKREF_ADC_SEL_CLKREF */
		fmspeedy_set_reg_field(AUX_CON3, 27, (0x0001<<27), 1); /* XTAL_EN_FM_BUF on */
	}

	/* Set up LO */
	fm_lo_set(radio->low->fm_tune_info.lo_setup);

	if (!radio->dual_clk_on) {
		if ((freq == 99900) || (freq == 100000) || (freq == 100100)) {
			fmspeedy_set_reg_field(AUX_CON0, 21, (0x0001<<21), 1); /* FMCLK_32M */
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 5, (0x0001 << 5), 1);
		} else {
			fmspeedy_set_reg_field(AUX_CON0, 21, (0x0001<<21), 0); /* FMCLK_40M */
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 5, (0x0001 << 5), 0);
		}
	}

#if 0
	/* Initialise I/Q imbalance */
	fm_setup_iq_imbalance();
#endif

	fmspeedy_set_reg_field(AUX_CON0, 21, (0x0001<<21), 0); /* FMCLK_40_32M, default = 1 */

	/* Set up Demod IF */
	fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 7, (0x0001<<7), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 8, (0x0001<<8), 0);
	if (radio->seek_status == FM_TUNER_PRESET_MODE)
		udelay(100);
	fmspeedy_set_reg_field(DIV_VAL, 0, (0x000F), 0x02);
	fmspeedy_set_reg_field(DIV_VAL, 4, (0x000F<<4), 0x09);
	if (radio->seek_status == FM_TUNER_PRESET_MODE)
		udelay(100);

	if (radio->dual_clk_on) {
		if (is_freq_in_spur(radio->low->fm_state.freq, fm_dual_clk_init, radio->dual_clk_on)) {
			fmspeedy_set_reg_field(AUX_CON0, 21, (0x0001<<21), 1); /* FMCLK_40_32M, default = 1 */
			/* Set up Demod IF */
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 7, (0x0001<<7), 1);
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 8, (0x0001<<8), 1);
			if (radio->seek_status == FM_TUNER_PRESET_MODE)
				udelay(100);
			fmspeedy_set_reg_field(DIV_VAL, 0, (0x000F), 0x03);
			fmspeedy_set_reg_field(DIV_VAL, 4, (0x000F<<4), 0x07);
			if (radio->seek_status == FM_TUNER_PRESET_MODE)
				udelay(100);
			dev_info(radio->dev, "7.5MHz Dual Clock ON [%06d]", radio->low->fm_state.freq);
		}
	}

	fmspeedy_set_reg(FM_DEMOD_IF, radio->low->fm_tune_info.rx_setup.demod_if);
	fmspeedy_set_reg(FM_DEMOD_FILTER_SELECT, 0x924); /* wide w te */

	if (radio->rssi_est_on) {
		fm_update_rssi(radio);
		if (radio->low->fm_state.rssi >= 176)
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 1);
	} else
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 0);

	if (radio->sw_mute_weak) {
		radio->low->fm_config.mute_coeffs_soft = 0x1B16;
		fmspeedy_set_reg(FM_DEMOD_SOFT_MUTE_COEFFS, radio->low->fm_config.mute_coeffs_soft);
	}

	/* change blending ref to RSSI */
	if (radio->low->fm_state.freq != 104000) {
		if (radio->rssi_ref_enable)
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 1);
		else
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 0);
	}

	/* FM ADC reset */
	fmspeedy_set_reg_field(FM_ENABLE, 5, (0x0001<<5), 0);
	fmspeedy_set_reg_field(FM_ENABLE, 5, (0x0001<<5), 1);

	/* FM DEMOD reset */
	fmspeedy_set_reg_field(FM_ENABLE, 0, (0x0001), 0);
	fmspeedy_set_reg_field(FM_ENABLE, 0, (0x0001), 1);

	FDEBUG(radio, "%s():seek_status:%d %d", __func__,
		radio->seek_status, radio->low->fm_state.tuner_mode);

	/* FM AGC config control */
	if (radio->seek_status == FM_TUNER_PRESET_MODE) {
		enable_agc_config_wbrssi(radio, TRUE);
		reset_agc_gain();
	} else {
		if (radio->agc_enable != AGC_CONFIG_WBRSSI_DISABLE)
			enable_agc_config_wbrssi(radio, FALSE);
	}

	if (radio->rds_parser_enable) {
		/* RDS parser reset */
		fm_rds_parser_reset(&(radio->pi));

		/* FIFO clear */
		for (ii = 0; ii < 32; ii++)
			fifo_tmp = fmspeedy_get_reg_work(FM_RDS_DATA);

#ifdef	USE_RINGBUFF_API
		radio->rds_rb.head = radio->rds_rb.tail = radio->rds_rb.buf;
#else	/* USE_RINGBUFF_API */
		radio->low->rds_buffer->index = radio->low->rds_buffer->outdex = 0;
#endif	/* USE_RINGBUFF_API */
	}

	API_EXIT(radio);
}
#elif defined(CONFIG_RADIO_S621)
#define NEW_TRF_ENABLE			(0x1 << 0)
#define NEW_TRF_ALWAYS_ADAPT		(0x1 << 1)
#define NEW_TRF_SECOND_ENABLE		(0x1 << 2)
#define NEW_TRF_SECOND_ALWAYS_ADAPT	(0x1 << 3)

int set_new_trj_ref_angle(struct s610_radio *radio, int tune_freq, int spur_freq[], int num_spur)
{
	int i;
	int result = 0;
	int spur_freq_sel[2];
	int diff[2] = {999999, 999999};
	int angle_first, angle_mid, angle_last;

	for (i = 0; i < num_spur; i++) {
		int quotient;
		int small, big;
		int spur_harmonic;
		int temp_diff;

		quotient = tune_freq / spur_freq[i];
		small = (spur_freq[i] * quotient);
		big = (spur_freq[i] * (quotient + 1));

		spur_harmonic = ((tune_freq - small) < (big - tune_freq)) ? small : big;
		temp_diff = spur_harmonic - tune_freq;

		if (ABS(diff[0]) > ABS(temp_diff)) {
			spur_freq_sel[1] = spur_freq_sel[0];
			diff[1] = diff[0];
			spur_freq_sel[0] = spur_freq[i];
			diff[0] = temp_diff;
		} else if ((ABS(diff[0]) != ABS(temp_diff)) && (ABS(diff[1]) > ABS(temp_diff))) {
			spur_freq_sel[1] = spur_freq[i];
			diff[1] = temp_diff;
		}
	}

	if ((diff[0] != 999999) && (ABS(diff[0]) <= 100) &&
			(ABS(diff[0]) != (spur_freq_sel[0] / 2))) {
		dev_info(radio->dev, "[%06d] new TRF enable for %dkHz harmonic spur",
			radio->low->fm_state.freq, spur_freq_sel[0]);
		result = NEW_TRF_ENABLE;
		if (diff != 0) {
			angle_first = ((diff[0] * 1000000000000000L) / 298023224) & 0x3fffffff;
			angle_mid = ((-diff[0] * 1000000000000000L) / 298023224) & 0x3fffffff;

			fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST, angle_first);
			fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE, angle_mid);
			result |= NEW_TRF_ALWAYS_ADAPT;
		} else
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x2);
	}

	if ((diff[1] != 999999) && (ABS(diff[1]) <= 100) &&
			(ABS(diff[1]) != (spur_freq_sel[1] / 2))) {
		dev_info(radio->dev, "[%06d] new TRF SECOND enable for %dkHz harmonic spur",
			radio->low->fm_state.freq, spur_freq_sel[1]);
		result |= NEW_TRF_SECOND_ENABLE;
		if (diff[1] != 0) {
			angle_mid = ((diff[1] * 1000000000000000) / 298023224) & 0x3fffffff;
			angle_last = ((-(diff[0] + diff[1]) * 1000000000000000) / 298023224) & 0x3fffffff;

			fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE, angle_mid);
			fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST, angle_last);
			result |= NEW_TRF_SECOND_ALWAYS_ADAPT;
		} else
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 20, (0x000F<<20), 0x2);
	}

	return result;
}

void fm_set_freq(struct s610_radio *radio, u32 freq, bool mix_hi)
{
	int ii;
	u32 fifo_tmp;

	API_ENTRY(radio);
	APIEBUG(radio, "set freq: %d", radio->low->fm_state.freq);

	radio->low->fm_tune_info.rx_setup.fm_freq_khz = freq;
	radio->low->fm_tune_info.rx_setup.fm_freq_hz = freq * 1000;

	if (radio->dual_clk_on
			&& is_freq_in_spur(radio->low->fm_state.freq, fm_dual_clk_init, radio->dual_clk_on)) {
		if (mix_hi) {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz + 225220;
			radio->low->fm_tune_info.rx_setup.demod_if = 0xF85;
		} else {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz - 225220;
			radio->low->fm_tune_info.rx_setup.demod_if = 0x7B;
		}
	} else {
		if (mix_hi) {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz + 224609;
			radio->low->fm_tune_info.rx_setup.demod_if = 0xF8D;
		} else {
			radio->low->fm_tune_info.lo_setup.rx_lo_req_freq =
				radio->low->fm_tune_info.rx_setup.fm_freq_hz - 224609;
			radio->low->fm_tune_info.rx_setup.demod_if = 0x73;
		}
	}

	fm_lo_prepare_setup(radio);

	if (freq <= 90000)
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x20;
	else
		radio->low->fm_tune_info.rx_setup.lna_cdac = 0x12;

	if (!radio->without_elna)
		radio->rssi_adjust = RSSI_ADJUST_WITHOUT_ELNA_VALUE;

	fmspeedy_set_reg(FM_DEMOD_RSSI_ADJUST,
		radio->low->fm_config.rssi_adj_ini+radio->rssi_adjust);
	APIEBUG(radio, "%s(): FM_DEMOD_RSSI_ADJUST: 0x%x 0x%x 0x%x", __func__,
			fmspeedy_get_reg(FM_DEMOD_RSSI_ADJUST),
			radio->low->fm_config.rssi_adj_ini, radio->rssi_adjust);

#ifdef	IDLE_POLLING_ENABLE
		fm_idle_periodic_cancel((unsigned long) radio);
#endif	/*IDLE_POLLING_ENABLE*/

	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001<<4), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 1);

	/* TRF FIRST */
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16), 0x6);

	/* TRF SECOND */
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 24, (0x0001<<24), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 25, (0x0001<<25), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 20, (0x000F<<20), 0x6);

	/* TRJ REF ANGLE */
	fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST, 0x0);
	fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE, 0x0);
	fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST, 0x0);

	if (radio->trf_on) {
		if (is_freq_in_spur(radio->low->fm_state.freq, fm_spur_trf_init,
				radio->trf_on)) {
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 1);
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001<<4), 1);
			if (radio->seek_status == FM_TUNER_PRESET_MODE)
				udelay(100);
			dev_info(radio->dev, "TRF ON [%06d][FM_DEMOD_CONFIG : 0x%08X]",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_CONFIG));
		}
	}

#ifdef	IDLE_POLLING_ENABLE
	fm_idle_periodic_update((unsigned long) radio);
#endif	/*IDLE_POLLING_ENABLE*/

	{	/* TRF */
#define NUM_CUSTOM_TRF	4
		bool is_custom_trf;
		u32 custom_trf[NUM_CUSTOM_TRF][6] = {
			/* freq, first angle, middle angle, last angle, bw1, bw2 */
			{88000, 0x10000000, 0x30000000, 0x0, 0x2, 0x0},
			{88100, 0x3C000000, 0x04000000, 0x0, 0x2, 0x0},
			{96000, 0x2C000000, 0x14000000, 0x0, 0x2, 0x0},
			{108000, 0x0, 0x0, 0x0, 0x2, 0x0},
		};

		is_custom_trf = false;
		for (ii = 0; ii < NUM_CUSTOM_TRF; ii++) {
			if (custom_trf[ii][0] == freq) {
				fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
				/* FIRST */
				fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
				fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
				fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 16, (0x000F<<16),
					custom_trf[ii][4]);
				/* SECOND, Set true bw2 for use second trf */
				if (custom_trf[ii][5] != 0x0) {
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 24, (0x0001<<24), 0x1);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 25, (0x0001<<25), 0x1);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 20, (0x000F<<20),
						custom_trf[ii][5]);
				}
				fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST, custom_trf[ii][1]);
				fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE, custom_trf[ii][2]);
				fmspeedy_set_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST, custom_trf[ii][3]);

				dev_info(radio->dev, "[%06d] new TRF(custom) On", radio->low->fm_state.freq);

				dev_info(radio->dev, "TRF [%06d] FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST : 0x%08X",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST));
				dev_info(radio->dev, "TRF [%06d] FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE : 0x%08X",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE));
				dev_info(radio->dev, "TRF [%06d] FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST : 0x%08X",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST));

				is_custom_trf = true;

				break;
			}
		}

		if (!is_custom_trf) {
			if (!is_freq_in_spur(radio->low->fm_state.freq, fm_spur_trf_init,
				radio->trf_on) && !is_freq_in_spur(radio->low->fm_state.freq,
					fm_dual_clk_init, radio->dual_clk_on)) {
				int ret;
				int spur_freq[2] = {320, 768};

				ret = set_new_trj_ref_angle(radio, radio->low->fm_state.freq,
					spur_freq, sizeof(spur_freq) / sizeof(int));
				if (NEW_TRF_ENABLE & ret) {
					dev_info(radio->dev, "[%06d] new TRF On",
						radio->low->fm_state.freq);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0x1);
					if (NEW_TRF_ALWAYS_ADAPT & ret)
						fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 8, (0x0001<<8), 0x1);
				}
				if (NEW_TRF_SECOND_ENABLE & ret) {
					dev_info(radio->dev, "[%06d] new TRF SECOND On",
						radio->low->fm_state.freq);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0x0);
					fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 24, (0x0001<<24), 0x1);
					if (NEW_TRF_SECOND_ALWAYS_ADAPT & ret)
						fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 25, (0x0001<<25), 0x1);
				}

				dev_info(radio->dev, "TRF [%06d] FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST : 0x%08X",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST));
				dev_info(radio->dev, "TRF [%06d] FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE : 0x%08X",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE));
				dev_info(radio->dev, "TRF [%06d] FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST : 0x%08X",
					radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST));

			}
		}
		dev_info(radio->dev, "TRF [%06d] FM_DEMOD_CONFIG : 0x%08X",
			radio->low->fm_state.freq, fmspeedy_get_reg(FM_DEMOD_CONFIG));
	}

#ifdef USE_SPUR_CANCEL
	if (radio->tc_on)
		fm_rx_check_spur(radio);
#endif

#ifdef USE_IQ_IMBAL_SMOOTH
	/* Clear the smooth config lock for IQ imbalance */
	fmspeedy_set_reg_field(FM_DEMOD_IMAGE_TRIM_SMOOTH_CONFIG, 6, (0x0001 << 6), 0);
#endif /*USE_IQ_IMBAL_SMOOTH*/

	/* Set up CDAC */
	fmspeedy_set_reg_field(RX_RF_CON2, 22, (0x003F << 22),
			radio->low->fm_tune_info.rx_setup.lna_cdac);

	if (freq == 104000) {
		fmspeedy_set_reg_field(RX_RF_CON3, 17, (0x0001<<17), 1); /* AUX_SEL_RX_ADC_CLK_30M  */
		fmspeedy_set_reg_field(PLL_CON2, 9, (0x0001<<9), 0); /* LO_CLKREF_ADC_SEL_CLKREF */
		fmspeedy_set_reg_field(AUX_CON, 18, (0x0001<<18), 0); /* XTAL_EN_FM_BUF off */
	} else {
		fmspeedy_set_reg_field(RX_RF_CON3, 17, (0x0001<<17), 0); /* AUX_SEL_RX_ADC_CLK_30M  */
		fmspeedy_set_reg_field(PLL_CON2, 9, (0x0001<<9), 1); /* LO_CLKREF_ADC_SEL_CLKREF */
		fmspeedy_set_reg_field(AUX_CON, 18, (0x0001<<18), 1); /* XTAL_EN_FM_BUF on */
	}

	/* Set up LO */
	fm_lo_set(radio->low->fm_tune_info.lo_setup);

	if (!radio->dual_clk_on) {
		if ((freq == 99900) || (freq == 100000) || (freq == 100100)) {
			fmspeedy_set_reg_field(BPLL_CON5, 3, (0x0007<<3), 4); /* FMCLK_30M */
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 5, (0x0001 << 5), 1);
		} else {
			fmspeedy_set_reg_field(BPLL_CON5, 3, (0x0007<<3), 7); /* FMCLK_40M */
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 5, (0x0001 << 5), 0);
		}
	}

#if 0
	/* Initialise I/Q imbalance */
	fm_setup_iq_imbalance();
#endif

	fmspeedy_set_reg_field(BPLL_CON5, 3, (0x0007<<3), 7); /* FMCLK_40M */

	/* Set up Demod IF */
	fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 6, (0x0001<<6), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 7, (0x0001<<7), 0);
	if (radio->seek_status == FM_TUNER_PRESET_MODE)
		udelay(100);
	fmspeedy_set_reg_field(CLK_SEL, 1, (0x0001<<1), 0);

	if (radio->dual_clk_on) {
		if (is_freq_in_spur(radio->low->fm_state.freq, fm_dual_clk_init, radio->dual_clk_on)) {
			fmspeedy_set_reg_field(BPLL_CON5, 3, (0x0007<<3), 4); /* FMCLK_30M */
			/* Set up Demod IF */
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 6, (0x0001<<6), 1);
			fmspeedy_set_reg_field(FM_DEMOD_CLOCK_RATE_CONFIG, 7, (0x0001<<7), 1);
			if (radio->seek_status == FM_TUNER_PRESET_MODE)
				udelay(100);
			fmspeedy_set_reg_field(CLK_SEL, 1, (0x0001<<1), 1);

			dev_info(radio->dev, "7.5MHz Dual Clock ON [%06d]", radio->low->fm_state.freq);
		}
	}

	fmspeedy_set_reg(FM_DEMOD_IF, radio->low->fm_tune_info.rx_setup.demod_if);
	fmspeedy_set_reg(FM_DEMOD_FILTER_SELECT, 0x924); /* wide w te */

	if (radio->rssi_est_on) {
		fm_update_rssi(radio);
		if (radio->low->fm_state.rssi >= 176)
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 1);
	} else
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 0);

	if (radio->sw_mute_weak) {
		radio->low->fm_config.mute_coeffs_soft = 0x1B16;
		fmspeedy_set_reg(FM_DEMOD_SOFT_MUTE_COEFFS, radio->low->fm_config.mute_coeffs_soft);
	}

	/* change blending ref to RSSI */
	if (radio->low->fm_state.freq != 104000) {
		if (radio->rssi_ref_enable)
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 1);
		else
			fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 0);
	}

	/* FM ADC reset */
	fmspeedy_set_reg_field(FM_ENABLE, 5, (0x0001<<5), 0);
	fmspeedy_set_reg_field(FM_ENABLE, 5, (0x0001<<5), 1);

	/* FM DEMOD reset */
	fmspeedy_set_reg_field(FM_ENABLE, 0, (0x0001), 0);
	fmspeedy_set_reg_field(FM_ENABLE, 0, (0x0001), 1);

	FDEBUG(radio, "%s():seek_status:%d %d", __func__,
		radio->seek_status, radio->low->fm_state.tuner_mode);

	/* FM AGC config control */
	if (radio->seek_status == FM_TUNER_PRESET_MODE) {
		enable_agc_config_wbrssi(radio, TRUE);
		reset_agc_gain();
	} else {
		if (radio->agc_enable != AGC_CONFIG_WBRSSI_DISABLE)
			enable_agc_config_wbrssi(radio, FALSE);
	}

	if (radio->rds_parser_enable) {
		/* RDS parser reset */
		fm_rds_parser_reset(&(radio->pi));

		/* FIFO clear */
		for (ii = 0; ii < 32; ii++)
			fifo_tmp = fmspeedy_get_reg_work(FM_RDS_DATA);

#ifdef	USE_RINGBUFF_API
		radio->rds_rb.head = radio->rds_rb.tail = radio->rds_rb.buf;
#else	/* USE_RINGBUFF_API */
		radio->low->rds_buffer->index = radio->low->rds_buffer->outdex = 0;
#endif	/* USE_RINGBUFF_API */
	}

	API_EXIT(radio);
}
#endif /* defined(CONFIG_RADIO_S610) */

void fm_set_mute(bool mute)
{
	if (mute)
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 0, 0x0001, 1); /* mute*/
	else
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 0, 0x0001, 0); /*unmute*/
}

void fm_set_blend_mute(struct s610_radio *radio)
{
	u16 mute_coeffs, blend_coeffs;

#ifdef MONO_SWITCH_INTERF
	if ((radio->low->fm_state.force_mono)
			|| (radio->low->fm_state.force_mono_interf)) {
#else
	if (radio->low->fm_state.force_mono) {
#endif
		blend_coeffs = radio->low->fm_config.blend_coeffs_dis;
	} else if (radio->low->fm_state.use_switched_blend) {
		/* Switched blend mode */
		blend_coeffs = radio->low->fm_config.blend_coeffs_switch;
	} else {
		/* Soft blend mode */
		blend_coeffs = radio->low->fm_config.blend_coeffs_soft;
	}

	if (radio->low->fm_state.use_soft_mute) {
		/* Soft mute */
		mute_coeffs = radio->low->fm_config.mute_coeffs_soft;
	} else {
		mute_coeffs = radio->low->fm_config.mute_coeffs_dis;
	}

	fmspeedy_set_reg(FM_DEMOD_STEREO_BLEND_COEFFS, blend_coeffs);
	fmspeedy_set_reg(FM_DEMOD_SOFT_MUTE_COEFFS, mute_coeffs);
}

static void fm_rds_flush_buffers(struct s610_radio *radio, bool clear_buffer)
{
	bool clear_sync = FALSE;

#ifdef	USE_RINGBUFF_API
		/* Clear the buffer pointers. */
		radio->rds_rb.head = radio->rds_rb.tail = radio->rds_rb.buf;
#else	/* USE_RINGBUFF_API */
	if (radio->low->rds_buffer != 0)
		/* Clear the buffer pointers. */
		radio->low->rds_buffer->index =
			radio->low->rds_buffer->outdex = 0;
#endif	/* USE_RINGBUFF_API */

	if (clear_buffer) {
		/* Diable RDS block */
		fmspeedy_set_reg_field(FM_ENABLE, 1, (0x0001 << 1), 0);
#ifndef RDS_POLLING_ENABLE
		/* Disable RDS int. */
		fm_set_interrupt_source((0x0001 << 4), FALSE);
#endif /* RDS_POLLING_ENABLE */
		/* Initialize the RDS state */
		radio->low->fm_rds_state.current_state = RDS_STATE_INIT;
		/* Clear the Sync flag after updating the status */
		clear_sync = TRUE;
		 /* Enable RDS block */
		fmspeedy_set_reg_field(FM_ENABLE, 1, (0x0001 << 1), 1);
#ifndef RDS_POLLING_ENABLE
		 /* Enable RDS int. */
		fm_set_interrupt_source((0x0001 << 4), TRUE);
#endif /* RDS_POLLING_ENABLE */
	}

	fm_clear_flag_bits(radio, FLAG_BUF_FUL);
	radio->low->fm_state.status &= ~STATUS_MASK_RDS_AVA;

	if (clear_sync)
		fm_update_rds_sync_status(radio, FALSE);
}

bool fm_radio_on(struct s610_radio *radio)
{
	u32 fm_en;

	API_ENTRY(radio);

	/* Start up analogue block */
	fm_rx_ana_start();

	/* Enable FM, DEMOD and ADC. */
	fm_en = fmspeedy_get_reg(FM_ENABLE);
#if defined(CONFIG_RADIO_S610)
	fmspeedy_set_reg(FM_ENABLE, (fm_en & 0x1DA));
	fmspeedy_set_reg(FM_ENABLE, (fm_en | 0x25));

	/* Clear int source */
	fmspeedy_set_reg(FM_INT_CLEAR, 0xFFFF);
#elif defined(CONFIG_RADIO_S621)
	fmspeedy_set_reg(FM_ENABLE, (fm_en & 0x18));
	fmspeedy_set_reg(FM_ENABLE, (fm_en | 0x25));

	/* Clear int source */
	fmspeedy_set_reg(FM_INT_CLEAR, 0x3FF);
#endif /* defined(CONFIG_RADIO_S610) */

	radio->low->fm_state.last_status_blend_stereo = FALSE;
	radio->low->fm_state.last_status_rds_sync = FALSE;

	API_EXIT(radio);
	/* Indicate success */
	return TRUE;
}

void fm_radio_off(struct s610_radio *radio)
{
	/* Disable all interrupt. */
	fm_set_interrupt_source(0xFFFF, FALSE);

	/* disable AudioOutEn */
	fm_audio_control(radio, 0, 0, 0, 0);

	/* Turn off FM digital block */
	fmspeedy_set_reg(FM_ENABLE, 0);

	/* Turn off analogue block */
	fm_rx_ana_stop();
}

void fm_rds_on(struct s610_radio *radio)
{
	memset(&radio->low->fm_rds_state, 0, sizeof(radio->low->fm_rds_state));

	/* Set the interrupt rate for RDS */
	fmspeedy_set_reg(FM_DEMOD_RDS_BYTE_COUNT_FOR_INT, radio->low->fm_config.rds_int_byte_count);
}

void fm_rds_off(struct s610_radio *radio)
{
	radio->low->fm_state.status &= ~STATUS_MASK_RDS_AVA;
}

void fm_rds_enable(struct s610_radio *radio)
{
#ifndef	USE_RDS_HW_DECODER
	u32 val = fmspeedy_get_reg(FM_RDS_DEC_CONFIG);
	u32 mask = ~0x2800;

	val &= mask;
	fmspeedy_set_reg(FM_RDS_DEC_CONFIG, val | 0x800);
#endif	/*USE_RDS_HW_DECODER*/
	fm_rds_flush_buffers(radio, TRUE);
}

void fm_rds_disable(struct s610_radio *radio)
{
	 /* Diable RDS block */
	fmspeedy_set_reg_field(FM_ENABLE, 1, (0x0001 << 1), 0);
	 /* Disable RDS int. */
	fm_set_interrupt_source((0x0001 << 4), FALSE);
	 /* Clear RDS sync. */
	fm_update_rds_sync_status(radio, FALSE);
}

/****************************************************************************/
/* Functions for the information management */
/****************************************************************************/

u16 fm_get_flags(struct s610_radio *radio)
{
	u16 resp = radio->low->fm_state.flags;

	fm_set_flags(radio, 0);

	return resp;
}

void fm_set_flags(struct s610_radio *radio, u16 flags)
{
	radio->low->fm_state.flags = flags;
}

void fm_update_if_count(struct s610_radio *radio)
{
	radio->low->fm_state.last_ifc = if_count_device_to_host(radio,
			fmspeedy_get_reg(FM_DEMOD_IF_COUNTER));
}

void fm_update_if_count_work(struct s610_radio *radio)
{
	radio->low->fm_state.last_ifc = if_count_device_to_host(radio,
			fmspeedy_get_reg_work(FM_DEMOD_IF_COUNTER));
}

void fm_update_rssi(struct s610_radio *radio)
{
	radio->low->fm_state.rssi =
		rssi_device_to_host(fmspeedy_get_reg(FM_DEMOD_DIG_RSSI),
		fmspeedy_get_reg(FM_AGC_GAIN_DEBUG), fmspeedy_get_reg(FM_DEMOD_RSSI_ADJUST));
}

void fm_update_rssi_work(struct s610_radio *radio)
{
	radio->low->fm_state.rssi =
		rssi_device_to_host(fmspeedy_get_reg_work(FM_DEMOD_DIG_RSSI),
		fmspeedy_get_reg_work(FM_AGC_GAIN_DEBUG), fmspeedy_get_reg_work(FM_DEMOD_RSSI_ADJUST));
}

void fm_update_snr(struct s610_radio *radio)
{
	radio->low->fm_state.snr = fmspeedy_get_reg(FM_DEMOD_SIGNAL_QUALITY);
}

void fm_update_sig_info(struct s610_radio *radio)
{
	fm_update_rssi(radio);
	fm_update_snr(radio);
}

void fm_update_rds_sync_status(struct s610_radio *radio, bool synced)
{
	if (radio->low->fm_state.last_status_rds_sync != synced) {
		if (synced != TRUE)
			fm_set_flag_bits(radio, FLAG_SYN_LOS);

		radio->low->fm_state.last_status_rds_sync = synced;
	}
}

#ifndef	USE_RDS_HW_DECODER
bool fm_get_rds_sync_status(struct s610_radio *radio)
{
	return radio->low->fm_state.last_status_rds_sync;
}
#endif	/*USE_RDS_HW_DECODER*/

u16 fm_update_rx_status(struct s610_radio *radio, u16 d_status)
{
	u16 flags = 0;
	u8 status = radio->low->fm_state.status	& ~STATUS_MASK_STEREO;
	bool blend_stereo = !!(d_status & FM_DEMOD_BLEND_STEREO_MASK);

	if (blend_stereo
		!= radio->low->fm_state.last_status_blend_stereo) {
		radio->low->fm_state.last_status_blend_stereo = blend_stereo;
		flags |= FLAG_CH_STAT;
	}
	if (blend_stereo)
		status |= STATUS_MASK_STEREO;

	radio->low->fm_state.status = status;

	return flags;
}

void fm_update_tuner_mode(struct s610_radio *radio)
{
	u8 tuner_mode = radio->low->fm_state.tuner_mode
			& ~TUNER_MODE_MASK_TUN_MOD;
	u32 tuner_state = (u32) radio->low->fm_tuner_state.tuner_state;

	switch (tuner_state) {
	case TUNER_OFF:
		tuner_mode |= TUNER_MODE_NONE;
		break;
	case TUNER_NOTTUNED:
		tuner_mode |= TUNER_MODE_NONE;
		break;
	case TUNER_IDLE:
		tuner_mode |= TUNER_MODE_NONE;
		break;
	case TUNER_PRESET:
		tuner_mode |= TUNER_MODE_PRESET;
		break;
	case TUNER_SEARCH:
		tuner_mode |= TUNER_MODE_SEARCH;
		break;
	default:
		break;
	}

	radio->low->fm_state.tuner_mode = tuner_mode;
}

bool fm_check_rssi_level(u16 limit)
{
	u16 d_rssi, gain, adjust;
	s16 rssi, thres;

	d_rssi = fmspeedy_get_reg(FM_DEMOD_DIG_RSSI);
	gain = fmspeedy_get_reg(FM_AGC_GAIN_DEBUG);
	adjust = fmspeedy_get_reg(FM_DEMOD_RSSI_ADJUST);

	rssi = rssi_device_to_host(d_rssi, gain, adjust);
	thres = aggr_rssi_device_to_host(limit);

	rssi = (rssi & 0x80) ? rssi - 256 : rssi;
	thres = (thres & 0x80) ? thres - 256 : thres;

	dev_info(gradio->dev, "%s rssi:%d thres:%d snr:%d",
		__func__, rssi, thres, fmspeedy_get_reg(FM_DEMOD_SIGNAL_QUALITY));

	return (rssi < thres);
}

/*******************************************************************/
int low_get_search_lvl(struct s610_radio *radio, u16 *value)
{
	*value =
		aggr_rssi_device_to_host(
			radio->low->fm_state.rssi_limit_search);

	return 0;
}
/*******************************************************************/
/* set function */
int low_set_if_limit(struct s610_radio *radio, u16 value)
{
	fmspeedy_set_reg(FM_DEMOD_IF_THRESHOLD, (u8) value);

	return 0;
}

int low_set_search_lvl(struct s610_radio *radio, u16 value)
{
	radio->low->fm_state.rssi_limit_search =
			aggr_rssi_host_to_device(value);
	fm_set_rssi_thresh(radio, radio->low->fm_tuner_state.tuner_state);

	return 0;
}

int low_set_freq(struct s610_radio *radio, u32 value)
{
	u32 freq = value;

	(void) fm_band_trim(radio, &freq);
	radio->low->fm_state.freq = freq;

	return 0;
}

int low_set_tuner_mode(struct s610_radio *radio, u16 value)
{
	API_ENTRY(radio);

	radio->low->fm_state.tuner_mode = value;
	radio->low->fm_tuner_state.curr_search_down =
			radio->low->fm_state.search_down;

	fm_set_tuner_mode(radio);

	radio->seek_status = value;
	FDEBUG(radio, "%s(), seek_status:%d %d", __func__, radio->seek_status, value);

	/* FM AGC config control */
	if (radio->seek_status == FM_TUNER_PRESET_MODE) {
		enable_agc_config_wbrssi(radio, TRUE);
		reset_agc_gain();
	}

	if (value == FM_TUNER_STOP_SEARCH_MODE) {
		/* Seek_cacel complete */
		complete(&radio->flags_seek_fr_comp);
		dev_info(radio->dev, ">>> send seek cancel complete");
	}

	API_EXIT(radio);

	return 0;
}

void fm_tuner_set_force_mute(struct s610_radio *radio, bool mute)
{
	radio->low->fm_state.mute_forced = mute;
	radio->low->fm_state.mute_audio = mute;
	fm_tuner_control_mute(radio);
}

int low_set_mute_state(struct s610_radio *radio, u16 value)
{
	/* Default set only fm stat initialie */
	/*radio->low->fm_state.use_soft_mute = !!(value & MUTE_STATE_MASK_SOFT);*/

	fm_tuner_set_force_mute(radio, !!(value & MUTE_STATE_MASK_HARD));
	fm_set_blend_mute(radio);

	return 0;
}

int low_set_most_mode(struct s610_radio *radio, u16 value)
{
	radio->low->fm_state.force_mono = !(value & MODE_MASK_MONO_STEREO);
	fm_set_blend_mute(radio);

	return 0;
}

int low_set_most_blend(struct s610_radio *radio, u16 value)
{
/*	radio->low->fm_state.use_switched_blend = !!(value & MODE_MASK_BLEND);*/
	fm_set_blend_mute(radio);

	return 0;
}

int low_set_pause_lvl(struct s610_radio *radio, u16 value)
{
	fmspeedy_set_reg(FM_DEMOD_AUDIO_PAUSE_THRESHOLD, (u8)(value & 0x00FF));

	return 0;
}

int low_set_pause_dur(struct s610_radio *radio, u16 value)
{
	fmspeedy_set_reg(FM_DEMOD_AUDIO_PAUSE_DURATION, (u8)(value & 0x3F));

	return 0;
}

int low_set_demph_mode(struct s610_radio *radio, u16 value)
{
	if (value & MODE_MASK_DEEMPH)
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 1, (0x0001 << 1), 1);
	else
		fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 1, (0x0001 << 1), 0);

	return 0;
}

int low_set_rds_cntr(struct s610_radio *radio, u16 value)
{
	if (value & RDS_CTRL_MASK_FLUSH)
		fm_rds_flush_buffers(radio, !!(value & RDS_CTRL_MASK_RESYNC));

	return 0;
}

int low_set_power(struct s610_radio *radio, u16 value)
{
	fm_tuner_set_power_state(radio,
			value & PWR_MASK_FM, value & PWR_MASK_RDS);

	return 0;
}

/****************************************************************************/
/* Functions for interrupt */
/****************************************************************************/
#ifndef	RDS_POLLING_ENABLE
void fm_set_handler_if_count(void (*fn)(struct s610_radio *radio))
{
	handler_if_count = fn;
	fm_set_interrupt_source(1, fn ? TRUE : FALSE);
}

void fm_set_handler_audio_pause(void (*fn)(struct s610_radio *radio))
{
	handler_audio_pause = fn;
	fm_set_interrupt_source((1 << 3), fn ? TRUE : FALSE);
}
#endif	/*RDS_POLLING_ENABLE*/

void fm_set_interrupt_source(u16 sources, bool enable)
{
	u32 mask;

	if (enable) {
		/*fmspeedy_set_reg(FM_INT_CLEAR, sources);*//* clear int. */
		 /* Get int. mask */
		mask = fmspeedy_get_reg(FM_INT_MASK);
		 /* Set Int. mask */
		fmspeedy_set_reg(FM_INT_MASK, (mask | sources));
	} else {
		 /* Get int. mask */
		mask = fmspeedy_get_reg(FM_INT_MASK);
		 /* Set Int. mask */
		fmspeedy_set_reg(FM_INT_MASK, (mask & ~sources));
	}
}

#ifdef	ENABLE_RDS_WORK_QUEUE
void s610_rds_work(struct work_struct *work)
{
	struct s610_radio *radio;

	radio = container_of(work, struct s610_radio, work);

	/*	FDEBUG(radio, ">R");*/
	fm_process_rds_data(radio);
}
#endif	/*ENABLE_RDS_WORK_QUEUE*/

#ifndef	RDS_POLLING_ENABLE
#ifdef	ENABLE_IF_WORK_QUEUE
void s610_if_work(struct work_struct *work)
{
	struct s610_radio *radio;

	radio = container_of(work, struct s610_radio, if_work);

	/*	FUNC_ENTRY(radio);*/
	FDEBUG(radio, ">IF");

	if (handler_if_count)
		(*handler_if_count)(radio);

	/*	FUNC_EXIT(radio);*/
}
#endif	/*ENABLE_IF_WORK_QUEUE*/
#endif	/* RDS_POLLING_ENABLE */

void s610_sig2_work(struct work_struct *work)
{
	struct s610_radio *radio;

	radio = container_of(work, struct s610_radio, dwork_sig2.work);

	FDEBUG(radio, ">S;%d, %d", radio->low->fm_config.search_conf.normal_ifca_m,
		radio->low->fm_config.search_conf.normal_ifca_h);
	fm_search_check_signal2((unsigned long) radio);
}

void s610_tune_work(struct work_struct *work)
{
	struct s610_radio *radio;

	radio = container_of(work, struct s610_radio, dwork_tune.work);

	FDEBUG(radio, ">T");
	fm_search_tuned((unsigned long) radio);
}

#ifdef	RDS_POLLING_ENABLE
#define RDS_MAX_FIFO	32
void s610_rds_poll_work(struct work_struct *work)
{
	struct s610_radio *radio;
	u32 fifo_status, rds_count;

	radio = container_of(work, struct s610_radio, dwork_rds_poll.work);

	if (radio->rds_flag == FM_RDS_ENABLE) {
		spin_lock_irq(&radio->slock);

		fm_update_rssi_work(radio);
		if (radio->low->fm_state.rssi < RDS_VALID_THRESHOLD) {
			RDSEBUG(radio, "RDS Current RSSI invalid!![%02d][%02d]",
				radio->low->fm_state.rssi, RDS_VALID_THRESHOLD);
			radio->invalid_rssi = TRUE;
			goto fm_periodic_update;
		}

#if defined(CONFIG_RADIO_S610)
		fifo_status = fmspeedy_get_reg_work(FM_SPEEDY_FIFO_STAT);
		if (fifo_status & 0x02)
			dev_info(radio->dev, ">>>>> RDS FIFO FULL restart RDS!!");

		rds_count = (fifo_status >> 8) & 0x3F;
#elif defined(CONFIG_RADIO_S621)
		fifo_status = fmspeedy_get_reg_work(FM_SPEEDY_STAT);
		if (fifo_status & (0x01 << 11))
			dev_info(radio->dev, ">>>>> RDS FIFO FULL restart RDS!!");

		rds_count = (fifo_status >> 12) & 0x3F;
#endif /* defined(CONFIG_RADIO_S610) */

		if (rds_count >= RDS_MAX_FIFO/4) {
			RDSEBUG(radio, "%s(): rds_count:%d fifo_status[%08X]",
				__func__, rds_count, fifo_status);
		fm_process_rds_data(radio);
		}

fm_periodic_update:
		fm_rds_periodic_update((unsigned long) radio);

		spin_unlock_irq(&radio->slock);
	}
}
#endif	/*RDS_POLLING_ENABLE*/

#ifdef	IDLE_POLLING_ENABLE
void s610_idle_poll_work(struct work_struct *work)
{
	struct s610_radio *radio;
	int rssi;

	radio = container_of(work, struct s610_radio, dwork_idle_poll.work);

	if (!wake_lock_active(&radio->wakelock))
		wake_lock(&radio->wakelock);

	spin_lock_irq(&radio->slock);

	fm_update_rssi_work(radio);
	rssi = radio->low->fm_state.rssi;
	rssi = (rssi & 0x80) ? rssi - 256 : rssi;
	APIEBUG(radio, "Current RSSI is [%02d][%02d] TRF ON/OFF RSSI[%2d:%2d] SNR ON/FF RSSI[%2d:%2d]",
			rssi, radio->low->fm_state.rssi,
			TRF_ON_RSSI_VALUE, TRF_OFF_RSSI_VALUE,
			SNR_ON_RSSI_VALUE, SNR_OFF_RSSI_VALUE);

	radio->low->fm_state.snr = fmspeedy_get_reg_work(FM_DEMOD_SIGNAL_QUALITY);
	APIEBUG(radio, "Current RSSI:[%02d][%02d] SNR:[%02d] Freq:[%8d]",
		rssi, radio->low->fm_state.rssi,
		radio->low->fm_state.snr,
		radio->low->fm_state.freq);

	if (radio->low->fm_state.freq == 104000) {
		/* TRF ON ? */
		if ((!fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 7, (0x0001<<7))) &&
				(fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 6, (0x0001<<6)))) {
			if (radio->low->fm_state.rssi >= TRF_OFF_RSSI_VALUE) {
				if (!fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 7, (0x0001<<7)))
					fmspeedy_set_reg_field_work(FM_DEMOD_CONFIG, 7, (0x0001<<7), 1);

				if (fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 6, (0x0001<<6)))
					fmspeedy_set_reg_field_work(FM_DEMOD_CONFIG, 6, (0x0001<<6), 0);
			}
		} else {	/* TRF OFF? */
			if ((fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 7, (0x0001<<7))) &&
					(!fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 6, (0x0001<<6)))) {
				if (radio->low->fm_state.rssi < TRF_ON_RSSI_VALUE) {
					if (fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 7, (0x0001<<7)))
						fmspeedy_set_reg_field_work(FM_DEMOD_CONFIG, 7, (0x0001<<7), 0);
					if (!fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 6, (0x0001<<6)))
						fmspeedy_set_reg_field_work(FM_DEMOD_CONFIG, 6, (0x0001<<6), 1);
				}
			}
		}

		/* SNR deviation bit 4 : 0 ? */
		if (!fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 4, (0x0001 << 4))) {
			if (radio->low->fm_state.rssi >= SNR_OFF_RSSI_VALUE)
				fmspeedy_set_reg_field_work(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 1);
		} else {
			/* SNR deviation bit 4 : 1 ? */
			if (radio->low->fm_state.rssi < SNR_ON_RSSI_VALUE)
				fmspeedy_set_reg_field_work(FM_DEMOD_CONFIG, 4, (0x0001 << 4), 0);
		}
	}

	APIEBUG(radio, ">>> FM_DEMOD_CONFIG bit[7:6:4] and FM_DEMOD_RSSI_ADJUST is [%02d:%02d:%02d][%04x]",
			fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 7, (0x0001<<7)),
			fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 6, (0x0001<<6)),
			fmspeedy_get_reg_field_work(FM_DEMOD_CONFIG, 4, (0x0001<<4)),
			fmspeedy_get_reg_work(FM_DEMOD_RSSI_ADJUST));

	fm_idle_periodic_update((unsigned long) radio);

	spin_unlock_irq(&radio->slock);

	if (wake_lock_active(&radio->wakelock))
		wake_unlock(&radio->wakelock);

}
#endif	/*IDLE_POLLING_ENABLE*/

#ifndef	RDS_POLLING_ENABLE
void fm_isr(struct s610_radio *radio)
{
	u16 cause;

	cause = fmspeedy_get_reg(FM_INT_CAUSE); /* save */

	fmspeedy_set_reg(FM_INT_CLEAR, cause); /* clear */
	udelay(10);

	cause &= fmspeedy_get_reg(FM_INT_MASK); /* mask */

	if (cause & INT_IFC_READY_MASK) {
		fmspeedy_set_reg(FM_INT_MASK,
			fmspeedy_get_reg(FM_INT_MASK) & 0xFFFE);
#ifdef	ENABLE_IF_WORK_QUEUE
		schedule_work(&radio->if_work);
#else
		if (handler_if_count)
			(*handler_if_count)(radio);

#endif	/*ENABLE_IF_WORK_QUEUE*/

	}

	if (cause & INT_RDS_BYTES_MASK) {
#ifdef	ENABLE_RDS_WORK_QUEUE
		schedule_work(&radio->work);
#else
		fm_process_rds_data(radio);
#endif	/*ENABLE_RDS_WORK_QUEUE*/
	}

	if (cause & INT_AUDIO_PAU_MASK) {
		fmspeedy_set_reg(FM_INT_MASK,
			fmspeedy_get_reg(FM_INT_MASK) & 0xFFF7);

		if (handler_audio_pause)
			(*handler_audio_pause)(radio);
	}
}
#endif	/* RDS_POLLING_ENABLE */

/****************************************************************************/
/* Functions for RX */
/****************************************************************************/

void fm_rx_ana_start(void)
{
	u32 adc_config1 = 0;

	/* ADC setting */
	adc_config1 = 0x01EF7A53;

	/* RF setting */
	fmspeedy_set_reg(RX_RF_CON1, 0xFC1CDFFF);
	/* fmspeedy_set_reg(RX_RF_CON3, 0x80988002); */
	fmspeedy_set_reg(RX_RF_CON3, 0x81788002);
	fmspeedy_set_reg(RX_RF_CON2, 0x040003FD);

	/* ADC input disconnect */
	fmspeedy_set_reg(ADC_CON2, 0);
	/* ADC enable */
	fmspeedy_set_reg(ADC_CON1, adc_config1);
	/* ADC reset */
	fmspeedy_set_reg(ADC_CON1, adc_config1 | (1 << 31) | (1 << 30));
	fmspeedy_set_reg(ADC_CON1, adc_config1);
	/* Overload block reset */
	fmspeedy_set_reg(ADC_CON1, adc_config1 | (1 << 25));
	fmspeedy_set_reg(ADC_CON1, adc_config1);
	/* ADC input connect */
	fmspeedy_set_reg(ADC_CON2, 2);
}

void fm_rx_ana_stop(void)
{
	fmspeedy_set_reg(ADC_CON1, 0);
	fmspeedy_set_reg(ADC_CON2, 0);
	fmspeedy_set_reg(RX_RF_CON1, 0);
	fmspeedy_set_reg(RX_RF_CON2, 0);
	fmspeedy_set_reg(RX_RF_CON3, 0);
}

void fm_setup_iq_imbalance(void)
{
#if defined(CONFIG_RADIO_S610)
	fmspeedy_set_reg_field(FM_DEMOD_IMAGE_TRIM_AMPLI, 0, 0x03FF, 511);
	fmspeedy_set_reg_field(FM_DEMOD_IMAGE_TRIM_PHASE, 0, 0x03FF, 511);
#elif defined(CONFIG_RADIO_S621)
	fmspeedy_set_reg_field(FM_DEMOD_IMAGE_TRIM_AMPLI, 0, 0x03FF, 0x01FF);
	fmspeedy_set_reg_field(FM_DEMOD_IMAGE_TRIM_PHASE, 0, 0x01FF, 0x01FF);
#endif /* defined(CONFIG_RADIO_S610) */
}

void fm_rx_init(void)
{
	/* Turn off analogue. */
	fm_rx_ana_stop();
}

#ifdef USE_SPUR_CANCEL
void fm_rx_en_spur_removal(struct s610_radio *radio)
{
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 13, (0x0001 << 13), 0);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 12, (0x0001 << 12), 1);
	fmspeedy_set_reg(FM_DEMOD_TONEREJ_FREQ, radio->low->fm_tune_info.rx_setup.spur_freq);
}

void fm_rx_dis_spur_removal(void)
{
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 13, (0x0001 << 13), 1);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 12, (0x0001 << 12), 0);
}

void fm_rx_check_spur(struct s610_radio *radio)
{
	u32 freq_gap_khz;
	u16 i;

	fm_rx_dis_spur_removal();

	for (i = 0; i < radio->tc_on; i++) {
		if (radio->low->fm_tune_info.rx_setup.fm_freq_khz
			>= radio->low->fm_spur[i]) {
			if ((radio->low->fm_tune_info.rx_setup.fm_freq_khz
				- radio->low->fm_spur[i]) < 160) {
				freq_gap_khz =
					radio->low->fm_tune_info.rx_setup.fm_freq_khz
					- radio->low->fm_spur[i];

				radio->low->fm_tune_info.rx_setup.spur_freq =
						(s16)((freq_gap_khz * 2048) / 10);
				fm_rx_en_spur_removal(radio);
				break;
			}
		} else {
			if ((radio->low->fm_spur[i]
				- radio->low->fm_tune_info.rx_setup.fm_freq_khz) < 160) {
				freq_gap_khz =
					radio->low->fm_spur[i]
					- radio->low->fm_tune_info.rx_setup.fm_freq_khz;

				radio->low->fm_tune_info.rx_setup.spur_freq =
						(s16)(((freq_gap_khz * 2048) / 10) * (-1));
				fm_rx_en_spur_removal(radio);
				break;
			}
		}

	}
}

void fm_rx_check_spur_mono(struct s610_radio *radio)
{
	if ((radio->low->fm_tune_info.rx_setup.spur_ctrl &
			DIS_SPUR_REMOVAL_MONO) &&
		(!fmspeedy_get_reg_field(FM_DEMOD_STATUS, 1, 0x0001 << 1))) {
		fm_rx_dis_spur_removal();

		/* Disable spur removal */
		radio->low->fm_tune_info.rx_setup.spur_ctrl = 0;
	}
}
#endif

/****************************************************************************/
/* Functions for LO */
/****************************************************************************/

void fm_lo_off(void)
{
	fmspeedy_set_reg(PLL_CON1, 0);
}

void fm_lo_prepare_setup(struct s610_radio *radio)
{
	u32 freq_hz;
	u32 fref;
	u32 flimit;
	s64 flodiv_prev, flodiv_cur;
	u16 ii;
	u32 n_lodiv;

	u64 fdco_t, ndiv_t, fcw_total = 0;
	u64 fdco_r;

	freq_hz = radio->low->fm_tune_info.lo_setup.rx_lo_req_freq;
	if (radio->low->fm_tune_info.rx_setup.fm_freq_khz == 104000)
		fref = 20000000;
	else
		fref = 26000000;

	flimit = 3000000000;

	/* Calculate the division value of LO divider */
	/* Look for the index of the smallest abs value */
	for (ii = 1; ii < 11; ii++) {
		flodiv_cur = ABS((int)(flimit - ((ii + 13) * freq_hz * 2)));
		if (ii == 1)
			flodiv_prev = flodiv_cur;

		n_lodiv = ii + 13;
		if (flodiv_cur > flodiv_prev) {
			n_lodiv -= 1;
			break;
		}
		flodiv_prev = flodiv_cur;
	}

	fdco_t = freq_hz * (u64) n_lodiv * 2;
	ndiv_t = fdco_t / (u64) fref;
	fdco_r = fdco_t % (u64) fref;

	ndiv_t *= (1 << 22);
	fdco_r = ((fdco_r * (1 << 22)) + (fref >> 1)) / (u64) fref;

	fcw_total = ndiv_t + fdco_r;

	radio->low->fm_tune_info.lo_setup.n_mmdiv = (u32)(
			(fcw_total >> 22) & 0x1FF);
	radio->low->fm_tune_info.lo_setup.frac_b1 = (u32)(
			((fcw_total % (1 << 22)) >> 10) & 0xFFF);
	radio->low->fm_tune_info.lo_setup.frac_b0 = (u32)(
			(fcw_total % (1 << 10)) & 0x3FF);
	radio->low->fm_tune_info.lo_setup.n_lodiv = n_lodiv;

}

void fm_lo_set(const struct fm_lo_setup lo_set)
{
	fmspeedy_set_reg(PLL_CON3,
			(1 << 21) | (lo_set.n_mmdiv << 12) | lo_set.frac_b1);
	fmspeedy_set_reg(PLL_CON4,
			(1 << 21)
			| (lo_set.frac_b0 << 11)
			| (lo_set.n_lodiv << 6)
			| 8);
	udelay(100);
}

void fm_lo_initialize(struct s610_radio *radio)
{
	API_ENTRY(gradio);

	fm_sx_reset();

	/* Set up the default PLL frequency */
	radio->low->fm_tune_info.lo_setup.rx_lo_req_freq = 76000000;

	/* Turn on the logic controller and dividers. */
	fm_sx_start();

	API_EXIT(gradio);
}

#if defined(CONFIG_RADIO_S610)
void fm_sx_reset(void)
{
	API_ENTRY(gradio);

	/* Reset the FM_SX registers */
	if (gradio->rfchip_ver == S620_REV_0) {
		fmspeedy_set_reg(PLL_CON1, 0x00B03E);
		fmspeedy_set_reg(PLL_CON2, 0x004600);
		fmspeedy_set_reg(PLL_CON3, 0x27365E);
		fmspeedy_set_reg(PLL_CON4, 0x10BDC8);
		fmspeedy_set_reg(PLL_CON5, 0x0A20D0);
		fmspeedy_set_reg(PLL_CON6, 0x018132);
		fmspeedy_set_reg(PLL_CON7, 0x065A78);
		fmspeedy_set_reg(PLL_CON8, 0x243100);
		fmspeedy_set_reg(PLL_CON9, 0x0C0518);
		fmspeedy_set_reg(PLL_CON10, 0);
		fmspeedy_set_reg(PLL_CON12, 0);
		fmspeedy_set_reg(PLL_CON13, 0x01F8F4);
		fmspeedy_set_reg(PLL_CON14, 0);
		fmspeedy_set_reg(PLL_CON15, 0);
		fmspeedy_set_reg(PLL_CON16, 0x26081D);
		fmspeedy_set_reg(PLL_CON17, 0);
		fmspeedy_set_reg(PLL_CON18, 0x2C0000);
		fmspeedy_set_reg(PLL_CON19, 0x040000);
		fmspeedy_set_reg(PLL_CON20, 0x008840);
	} else/* if (gradio->rfchip_ver == S612_REV_1)*/ {
		fmspeedy_set_reg(PLL_CON1, 0x009020);
		fmspeedy_set_reg(PLL_CON2, 0x004600);
		fmspeedy_set_reg(PLL_CON3, 0x27365E);
		fmspeedy_set_reg(PLL_CON4, 0x10BDC8);
		fmspeedy_set_reg(PLL_CON5, 0x24A0D0);
		fmspeedy_set_reg(PLL_CON6, 0x018132);
		fmspeedy_set_reg(PLL_CON7, 0x065A78);
		fmspeedy_set_reg(PLL_CON8, 0x243100);
		fmspeedy_set_reg(PLL_CON9, 0x0C0518);
		fmspeedy_set_reg(PLL_CON10, 0);
		fmspeedy_set_reg(PLL_CON12, 0);
		fmspeedy_set_reg(PLL_CON13, 0x01F8F4);
		fmspeedy_set_reg(PLL_CON14, 0);
		fmspeedy_set_reg(PLL_CON15, 0);
		fmspeedy_set_reg(PLL_CON16, 0x26081D);
		fmspeedy_set_reg(PLL_CON17, 0);
		fmspeedy_set_reg(PLL_CON18, 0x2C0000);
		fmspeedy_set_reg(PLL_CON19, 0x040000);
		fmspeedy_set_reg(PLL_CON20, 0x00883C);
	}

	API_EXIT(gradio);
}

void fm_sx_start(void)
{
	API_ENTRY(gradio);

	if (gradio->rfchip_ver == S620_REV_0) {
		fmspeedy_set_reg(PLL_CON20, 0x0F8840);
		udelay(50);
		fmspeedy_set_reg(PLL_CON1, 0x3CB03E);
		udelay(20);
		fmspeedy_set_reg(PLL_CON20, 0x0F8840);
		fmspeedy_set_reg(PLL_CON1, 0x14B03E);
	} else/* if (gradio->rfchip_ver == S612_REV_1)*/ {
		fmspeedy_set_reg(PLL_CON20, 0x0F883C);
		udelay(50);
		fmspeedy_set_reg(PLL_CON1, 0x3C9020);
		udelay(20);
		fmspeedy_set_reg(PLL_CON20, 0x0B883C);
		fmspeedy_set_reg(PLL_CON1, 0x149020);
	}

	API_ENTRY(gradio);
}

bool fm_aux_pll_initialize(void)
{
	u32 pll_locked = 0;
	u16 i;

	API_ENTRY(gradio);

	fmspeedy_set_reg_field(PLL_CLK_EN, 0, 0x0001, 0); /* PLL_CLK_EN, default = 1 */
	udelay(20);
	dev_info(gradio->dev, "%s():PLL_CLK_EN[%02X]",
		__func__,
		fmspeedy_get_reg_field(PLL_CLK_EN, 0, 0x0001));

	fmspeedy_set_reg_field(AUX_CON0, 10, (0x0001<<10), 1); /* FMCLK_from240M, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 21, (0x0001<<21), 0); /* FMCLK_40_32M, default = 1 */
	fmspeedy_set_reg_field(AUX_CON0, 22, (0x0001<<22), 0); /* FMCLK_from260M, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 30, (0x0001<<30), 1); /* PLL_LOCK_EN, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 28, (0x0001<<28), 0); /* PLL_FEED_EN, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 26, (0x0001<<26), 1); /* PLL_EN_CLK_240M, default = 1 */
	fmspeedy_set_reg_field(AUX_CON0, 25, (0x0001<<25), 1); /* PLL_EN_CLK_120M, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 27, (0x0001<<27), 1); /* PLL_EN_CLK_80M, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 24, (0x0001<<24), 1); /* PLL_EN, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 23, (0x0001<<23), 1); /* OUT_EN, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 16, (0x0001<<16), 1); /* IREF_EN, default = 1 */
	fmspeedy_set_reg_field(AUX_CON0, 9, (0x0001<<9), 1); /* BUFFER_AD_EN, default = 1 */
	fmspeedy_set_reg_field(AUX_CON0, 29, (0x0001<<29), 0); /* PLL_FSEL, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 17, (0x000F<<17), 8); /* IREFTRIM, default = 8 */
	fmspeedy_set_reg_field(AUX_CON0, 14, (0x0003<<14), 0); /* IO_SPARE, default = 0 */
	fmspeedy_set_reg_field(AUX_CON0, 11, (0x0007<<11), 0); /* IO_OUT_SEL, default = 0 */

	fmspeedy_set_reg(AUX_CON1, 0x21F8570);
	if (gradio->rfchip_ver == S620_REV_0)
		fmspeedy_set_reg(AUX_CON2, 0x1F864201);
	else /*if (gradio->rfchip_ver == S612_REV_1)*/
		fmspeedy_set_reg(AUX_CON2, 0x1F864301);

	fmspeedy_set_reg(AUX_CON3, 0xD06C350);
	fmspeedy_set_reg(AUX_CON4, 0x21);

	for (i = 0; i < 10; i++) {
		udelay(300);
		pll_locked = fmspeedy_get_reg_field(AUX_CON6, 4, (0x0001 << 4));
		if (pll_locked) {
			dev_info(gradio->dev, "API> Aux pll lock!!");
			return TRUE;
		}
	}
	dev_err(gradio->dev, "Fail aux pll lock. Check RF power!!");
	print_ana_register();

	API_EXIT(gradio);

	return FALSE;
}
#elif defined(CONFIG_RADIO_S621)
void fm_sx_reset(void)
{
	API_ENTRY(gradio);

	/* Reset the FM_SX registers */
	fmspeedy_set_reg(PLL_CON1,  0x00000020);
	fmspeedy_set_reg(PLL_CON10, 0x0);
	fmspeedy_set_reg(PLL_CON12, 0x0);
	fmspeedy_set_reg(PLL_CON13, 0x0001F8F4);
	fmspeedy_set_reg(PLL_CON14, 0);
	fmspeedy_set_reg(PLL_CON15, 0);
	fmspeedy_set_reg(PLL_CON16, 0x0026081D);
	fmspeedy_set_reg(PLL_CON17, 0);
	fmspeedy_set_reg(PLL_CON18, 0x002C0000);
	fmspeedy_set_reg(PLL_CON19, 0x00040000);
	fmspeedy_set_reg(PLL_CON2,  0x00004600);
	fmspeedy_set_reg(PLL_CON20, 0x00048840);
	fmspeedy_set_reg(PLL_CON3,  0x0027365E);
	fmspeedy_set_reg(PLL_CON4,  0x010BDC8);
	fmspeedy_set_reg(PLL_CON5,  0x000A20D0);
	fmspeedy_set_reg(PLL_CON6, 0x00018132);
	fmspeedy_set_reg(PLL_CON7, 0x00065A78);
	fmspeedy_set_reg(PLL_CON8, 0x00243100);
	fmspeedy_set_reg(PLL_CON9, 0x000C0518);

	API_EXIT(gradio);
}

void fm_sx_start(void)
{
	API_ENTRY(gradio);

	fmspeedy_set_reg(PLL_CON20, 0x000F8840);
	fmspeedy_set_reg(PLL_CON1,  0x00180020);
	fmspeedy_set_reg(PLL_CON1,  0x00100020);

	API_ENTRY(gradio);
}

bool fm_aux_pll_initialize(void)
{
	u32 pll_locked = 0;
	u16 i;

	API_ENTRY(gradio);

	fmspeedy_set_reg(AUX_CON, 0xF8068708);
	fmspeedy_set_reg(BPLL_CON1, 0x0063D0FB);
	fmspeedy_set_reg(BPLL_CON2, 0x06FF9800);
	fmspeedy_set_reg(BPLL_CON4, 0x00C6024F);
	fmspeedy_set_reg(BPLL_CON5, 0x00018D79);
	fmspeedy_set_reg(BPLL_CON7, 0x00177008);
	fmspeedy_set_reg(BPLL_CON8, 0x00C00000);
	fmspeedy_set_reg(BPLL_CON9, 0X03400000);
	fmspeedy_set_reg(BPLL_CON3, 0x00066074);
	udelay(50);
	fmspeedy_set_reg_field(BPLL_CON3, 0, (0x0001<<0), 1); /* BPLL_CON3_ABC_START, default = 0 */

	for (i = 0; i < 10; i++) {
		udelay(300);
		pll_locked = fmspeedy_get_reg_field(BPLL_CON6, 6, (0x0001 << 6));
		if (pll_locked) {
			dev_info(gradio->dev, "API> Aux pll lock!!");
			return TRUE;
		}
	}
	dev_err(gradio->dev, "Fail aux pll lock. Check RF power!!");
	print_ana_register();

	API_EXIT(gradio);

	return FALSE;
}
#endif /* defined(CONFIG_RADIO_S610) */

void fm_ds_set(u32 data)
{
	fmspeedy_set_reg_field(FM_SPEEDY_PAD_CTRL, 7, (0x0003 << 7), data);
	msleep(10);
	dev_info(gradio->dev,
		"%s: DS set: 0x%xh, reg val: 0x%xh\n", __func__,
		data, fmspeedy_get_reg(FM_SPEEDY_PAD_CTRL));
}

void fm_get_version_number(void)
{
	u32 chip_ver;

#if defined(CONFIG_RADIO_S610)
	chip_ver = fmspeedy_get_reg(FM_DEMOD_VERSION);
	chip_ver = fmspeedy_get_reg(FM_TOP_VERSION);
	msleep(1);
	chip_ver = fmspeedy_get_reg(FM_TOP_VERSION);
#elif defined(CONFIG_RADIO_S621)
	chip_ver = fmspeedy_get_reg(FM_SPEEDY_STAT);
	chip_ver = fmspeedy_get_reg(FM_VERSION);
	msleep(1);
	chip_ver = fmspeedy_get_reg(FM_VERSION);
#endif /* defined(CONFIG_RADIO_S610) */
	dev_info(gradio->dev,
			">>> FM version: DRV:[%08X] HW:[%08X] DS:[%08X] BUILD:[%08X]",
			gradio->rfchip_ver,
			chip_ver,
			fmspeedy_get_reg_field(FM_SPEEDY_PAD_CTRL, 7, (0x0003 << 7)),
			build_identifier_integer);
}

void fm_aux_pll_off(void)
{
#if defined(CONFIG_RADIO_S610)
	fmspeedy_set_reg(AUX_CON0, 0x04300308);
	fmspeedy_set_reg(AUX_CON1, 0);
	fmspeedy_set_reg(AUX_CON2, 0);
	fmspeedy_set_reg(AUX_CON3, 0x05040000);
	fmspeedy_set_reg(AUX_CON4, 0x21);

	fmspeedy_set_reg_field(PLL_CLK_EN, 0, 0x0001, 1); /* PLL_CLK_EN, default = 1 */
	dev_info(gradio->dev, "%s():PLL_CLK_EN[%02X]",
		__func__,
		fmspeedy_get_reg_field(PLL_CLK_EN, 0, 0x0001));
#elif defined(CONFIG_RADIO_S621)
	fmspeedy_set_reg(AUX_CON, 0x04300308);
	fmspeedy_set_reg(BPLL_CON1, 0);
	fmspeedy_set_reg(BPLL_CON2, 0);
	fmspeedy_set_reg(BPLL_CON3, 0x05040000);
	fmspeedy_set_reg(BPLL_CON4, 0x21);
#endif /* defined(CONFIG_RADIO_S610) */
}

/****************************************************************************/
/* Functions for tuning */
/*****************************************************************************/

void fm_set_band(struct s610_radio *radio, u8 index)
{
	u16 num_of_bands = 0;

	num_of_bands = sizeof(radio->low->fm_bands) / sizeof(struct fm_band_s);

	if (index >= num_of_bands)
		index = num_of_bands - 1;

	radio->low->fm_state.band = index;

	radio->low->fm_tuner_state.band_limit_lo =
			radio->low->fm_bands[radio->low->fm_state.band].lo;
	radio->low->fm_tuner_state.band_limit_hi =
			radio->low->fm_bands[radio->low->fm_state.band].hi;

	radio->low->fm_state.freq = radio->low->fm_tuner_state.band_limit_lo;
}

void fm_set_freq_step(struct s610_radio *radio, u8 index)
{
	radio->low->fm_tuner_state.freq_step = radio->low->fm_freq_steps[index];
}

bool fm_band_trim(struct s610_radio *radio, u32 *freq)
{
	bool bl = FALSE;

	if (*freq <= radio->low->fm_tuner_state.band_limit_lo) {
		*freq = radio->low->fm_tuner_state.band_limit_lo;
		bl = TRUE;
	}
	if (*freq >= radio->low->fm_tuner_state.band_limit_hi) {
		*freq = radio->low->fm_tuner_state.band_limit_hi;
		bl = TRUE;
	}

	return bl;
}

static bool fm_tuner_push_freq(struct s610_radio *radio, bool down)
{
	u32 new_freq = radio->low->fm_state.freq;
	bool in_bl;

	if (down)
		new_freq -= radio->low->fm_tuner_state.freq_step;
	else
		new_freq += radio->low->fm_tuner_state.freq_step;

	in_bl = fm_band_trim(radio, &new_freq);
	radio->low->fm_state.freq = new_freq;

	return in_bl;
}

static void fm_tuner_enable_rds(struct s610_radio *radio, bool enable)
{
	if (radio->low->fm_state.rds_pwr_on) {
		if (enable && !radio->low->fm_state.rds_rx_enabled)
			fm_rds_enable(radio);
		else if (!enable && radio->low->fm_state.rds_rx_enabled)
			fm_rds_disable(radio);
	}
	radio->low->fm_state.rds_rx_enabled = enable;
}

void fm_set_rssi_thresh(struct s610_radio *radio, enum fm_tuner_state_low state)
{
	switch (state) {
	case TUNER_SEARCH:
		fmspeedy_set_reg(FM_DEMOD_RSSI_THRESHOLD,
			radio->low->fm_state.rssi_limit_search);
		break;
	case TUNER_IDLE:
	case TUNER_PRESET:
	default:
		fmspeedy_set_reg(FM_DEMOD_RSSI_THRESHOLD,
			radio->low->fm_state.rssi_limit_normal);
		break;
	}
}

static void fm_tuner_control_mute(struct s610_radio *radio)
{
	bool mute = radio->low->fm_state.mute_forced
			|| radio->low->fm_state.mute_audio
			|| (!radio->low->fm_tuner_state.tune_done);
	if (mute)
		fmspeedy_set_reg(FM_DEMOD_SOFT_MUTE_COEFFS, 0x2516);
	else
		fmspeedy_set_reg(FM_DEMOD_SOFT_MUTE_COEFFS, radio->low->fm_config.mute_coeffs_soft);

	udelay(100);
	fm_set_mute(mute);
}

void fm_tuner_set_mute_audio(struct s610_radio *radio, bool mute)
{
	radio->low->fm_state.mute_audio = mute;
	fm_tuner_control_mute(radio);
}

#ifdef MONO_SWITCH_INTERF
void fm_reset_force_mono_interf(struct s610_radio *radio)
{
	radio->low->fm_state.force_mono_interf = FALSE;
	radio->low->fm_state.mono_interf_reset_time = get_time();
	radio->low->fm_state.interf_checked = FALSE;
	fm_set_blend_mute(radio);
}

void fm_check_interferer(struct s610_radio *radio)
{
	s16 rssi;
	bool old_state = radio->low->fm_state.force_mono_interf;
	TIME passed_time =
			get_time()
			- radio->low->fm_state.mono_interf_reset_time;

	if (!radio->low->fm_state.interf_checked) {
		if (passed_time < (1 * SECOND))
			return;

			radio->low->fm_state.interf_checked = TRUE;
			radio->low->fm_state.mono_interf_reset_time =
					get_time();
			radio->low->fm_state.mono_switched_interf =
			FALSE;
		}

	rssi = (radio->low->fm_state.rssi & 0x80) ?
			radio->low->fm_state.rssi - 256 :
			radio->low->fm_state.rssi;
	if ((rssi > radio->low->fm_config.interf_rssi.hi)
			&& (radio->low->fm_state.snr
			< radio->low->fm_config.interf_snr.lo)) {
		radio->low->fm_state.force_mono_interf = TRUE;
		radio->low->fm_state.mono_switched_interf = TRUE;
	} else if ((rssi < radio->low->fm_config.interf_rssi.lo)
			|| (radio->low->fm_state.snr >
			radio->low->fm_config.interf_snr.hi)) {
		radio->low->fm_state.force_mono_interf = FALSE;
	}

	if (old_state != radio->low->fm_state.force_mono_interf)
		fm_set_blend_mute(radio);
}
#endif /* MONO_SWITCH_INTERF */

void fm_start_if_counter(void)
{
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 5, (0x0001 << 5), 0);
	udelay(4);
	fmspeedy_set_reg_field(FM_DEMOD_CONFIG, 5, (0x0001 << 5), 1);
}

static void fm_preset_tuned(struct s610_radio *radio)
{
	u16 count;
	u16 flag;

	API_ENTRY(radio);

	count = (fmspeedy_get_reg(FM_DEMOD_IF_COUNTER_INT_TIME) * 5) / 10;
	if (count > 20)
		print_ana_register();

	fm_tuner_enable_rds(radio, TRUE);
	fm_start_if_counter();

	fmspeedy_set_reg_field(FM_INT_CLEAR, 0, 1, 1); /* Clear Int. */
	msleep(20);

	flag = fm_update_rx_status(radio, fmspeedy_get_reg(FM_DEMOD_STATUS));

	fm_update_if_count(radio);
	fm_update_sig_info(radio);

	fm_tuner_exit_state(radio);
	fm_tuner_change_state(radio, TUNER_IDLE);
	if (radio->low->fm_tuner_state.hit_band_limit)
		flag |= FLAG_BD_LMT;
	fm_set_flag_bits(radio, flag | FLAG_TUNED);
	/*Set freq completed */
	complete(&radio->flags_set_fr_comp);
	APIEBUG(radio, ">>>>> preset tune complete!! 0x%x",
			radio->low->fm_state.freq);

	API_EXIT(radio);
}

static void fm_search_done(struct s610_radio *radio, u16 flags)
{
	API_ENTRY(radio);
	fm_tuner_set_mute_audio(radio, FALSE); /* unmute */

	fm_tuner_exit_state(radio);
	fm_tuner_change_state(radio, TUNER_IDLE);
	APIEBUG(radio, "API 01> Seek done doing complete!! 0x%x, flag 0x%x",
			radio->low->fm_state.freq, flags);

	fm_set_flag_bits(radio, flags | FLAG_TUNED);
	radio->irq_flag = flags;

	/* Seek_done complete */
	complete(&radio->flags_seek_fr_comp);

	fm_tuner_enable_rds(radio, TRUE);
	API_EXIT(radio);
}

static void fm_search_check_signal2(unsigned long data)
{
	static u16 min_weak_ifc_abs;
	static u16 min_normal_ifc_abs;
	static u16 retry_count;
	int rssi;
	u16 d_rssi, gain, adjust, snr;
	bool check_ok = TRUE;
	bool done = FALSE;
	u16 ifc_abs;
	struct s610_radio *radio = (void *) data;

	API_ENTRY(radio);

	if (radio->sig2_fniarg) {
		retry_count = 0;
		min_weak_ifc_abs = 0xffff;
		min_normal_ifc_abs = 0xffff;
	}

	d_rssi = fmspeedy_get_reg(FM_DEMOD_DIG_RSSI);
	gain = fmspeedy_get_reg(FM_AGC_GAIN_DEBUG);
	adjust = fmspeedy_get_reg(FM_DEMOD_RSSI_ADJUST);
	snr = fmspeedy_get_reg(FM_DEMOD_SIGNAL_QUALITY);
	rssi = rssi_device_to_host(d_rssi, gain, adjust);
	rssi = (rssi & 0x80) ? rssi - 256 : rssi;

	APIEBUG(radio, "SIG2> rssi %d, snr %d", rssi, snr);
	APIEBUG(radio, "SIG2> weak ifca_m %d", radio->low->fm_config.search_conf.weak_ifca_m);
	APIEBUG(radio, "SIG2> normal ifca_m %d", radio->low->fm_config.search_conf.normal_ifca_m);
	APIEBUG(radio, "SIG2> RSSI limit %d %d",
		aggr_rssi_device_to_host(radio->low->fm_state.rssi_limit_search),
		radio->low->fm_state.rssi_limit_search);
	APIEBUG(radio, "SIG2> with eLNA %d RSSI ADJUST %x %x",
		radio->without_elna, RSSI_ADJUST_WITHOUT_ELNA_VALUE, radio->low->fm_config.rssi_adj_ini);
	APIEBUG(radio, "SIG2> seek weak rssi %d", radio->seek_weak_rssi);
		dev_info(radio->dev, "SIG2> Current Freq %d", radio->low->fm_state.freq);

	if (rssi < radio->seek_weak_rssi)
		radio->low->fm_config.search_conf.weak_sig = TRUE;
	else
		radio->low->fm_config.search_conf.weak_sig = FALSE;

	if (!done) {
		ifc_abs = fmspeedy_get_reg(FM_DEMOD_IF_COUNTER_ABS);

		if (radio->low->fm_config.search_conf.weak_sig) {
			if (ifc_abs < radio->low->fm_config.search_conf.weak_ifca_l) {
				APIEBUG(radio, "SIG2> weak good %d", ifc_abs);
				done = TRUE;
				check_ok = TRUE;
			}

			else if (ifc_abs > radio->low->fm_config.search_conf.weak_ifca_h) {
				APIEBUG(radio, "SIG2> weak bad %d", ifc_abs);
				done = TRUE;
				check_ok = FALSE;
			}

			else if (ifc_abs < min_weak_ifc_abs) {
				APIEBUG(radio, "SIG2> weak mid %d %d", ifc_abs, min_weak_ifc_abs);
				min_weak_ifc_abs = ifc_abs;
			}
		} else {
			if (ifc_abs < radio->low->fm_config.search_conf.normal_ifca_l) {
				APIEBUG(radio, "SIG2> normal good %d", ifc_abs);
				done = TRUE;
				check_ok = TRUE;
			}

			else if (ifc_abs > radio->low->fm_config.search_conf.normal_ifca_h) {
				APIEBUG(radio, "SIG2> normal bad %d", ifc_abs);
				done = TRUE;
				check_ok = FALSE;
			}

			else if (ifc_abs < min_normal_ifc_abs) {
				APIEBUG(radio, "SIG2> normal mid %d %d", ifc_abs, min_normal_ifc_abs);
				min_normal_ifc_abs = ifc_abs;
			}
		}
	}

	if ((!done) && (++retry_count >= 5)) {
		done = TRUE;

		if (((min_weak_ifc_abs < 0xffff)
			&& (min_weak_ifc_abs > radio->low->fm_config.search_conf.weak_ifca_m))
			|| ((min_normal_ifc_abs < 0xffff)
			&& (min_normal_ifc_abs > radio->low->fm_config.search_conf.normal_ifca_m))) {
			check_ok = FALSE;
			APIEBUG(radio, "SIG2> check mid fail");
		}
	}

	if (done) {
		if (check_ok) {
			u16 flag =
				fm_update_rx_status(radio,
					fmspeedy_get_reg(FM_DEMOD_STATUS));

			fm_update_if_count(radio);
			fm_update_sig_info(radio);
			fm_search_done(radio, flag);
		} else {
			fm_search_check_signal1(radio, TRUE);
		}

	} else {
		radio->sig2_fniarg = FALSE;
		radio->dwork_sig2_counter++;
		schedule_delayed_work(&radio->dwork_sig2,
			msecs_to_jiffies(SEARCH_DELAY_MS));
	}

	API_EXIT(radio);
}

static void fm_search_check_signal1(struct s610_radio *radio, bool rssi_oor)
{
	u16 flag;
	u16 d_status;

	/*	API_ENTRY(radio);*/

	if (radio->seek_status == FM_TUNER_STOP_SEARCH_MODE) {
		/* Seek_cacel*/
		dev_info(radio->dev, "%s() Seek cancel", __func__);
		fm_search_done(radio, 0);

		return;
	}

#ifndef	RDS_POLLING_ENABLE
	fm_set_handler_if_count(NULL);
#endif	/* RDS_POLLING_ENABLE */

	d_status = fmspeedy_get_reg(FM_DEMOD_STATUS);
	APIEBUG(radio, "SIG1> rssi_oor : %d, d_status : 0x%x\n", rssi_oor?1:0, d_status);
	if (rssi_oor || !!(d_status & (0x0001 << 7))) {
		APIEBUG(radio, "SIG1> next freq");
		if (radio->low->fm_tuner_state.hit_band_limit) {
			APIEBUG(radio, "SIG1> IF_OOR 0x%x,0x%x,0x%x",
					radio->wrap_around, radio->seek_freq,
					radio->low->fm_state.freq);
			if (radio->wrap_around) {
				if (radio->seek_freq == radio->low->fm_state.freq) {
					flag = fm_update_rx_status(radio, d_status);
					fm_update_if_count(radio);
					fm_update_sig_info(radio);

					fm_search_done(radio, flag);
				} else {
					radio->low->fm_tuner_state.hit_band_limit =
							fm_tuner_push_freq(
									radio,
									radio->low->fm_tuner_state.curr_search_down);
					/* disable audio out */
#if defined(CONFIG_RADIO_S610)
					fm_audio_control(radio, 0, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
					fm_audio_control(radio, 0, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */

					fm_set_freq(radio, radio->low->fm_state.freq, 1);

					/* enable audio out */
#if defined(CONFIG_RADIO_S610)
					fm_audio_control(radio, 1, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
					fm_audio_control(radio, 1, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */

					radio->tune_fniarg = 0;
					radio->dwork_tune_counter++;
					schedule_delayed_work(&radio->dwork_tune,
							msecs_to_jiffies(0));
				}
			} else {
				flag = fm_update_rx_status(radio, d_status);
				fm_update_if_count(radio);
				fm_update_sig_info(radio);

				if (radio->seek_freq == radio->low->fm_state.freq)
					fm_search_done(radio, flag);
				else
					fm_search_done(radio, flag | FLAG_BD_LMT);

				APIEBUG(radio, "SIG1> IF_Not OOR 0x%x,0x%x,0x%x,0x%x",
						radio->wrap_around, radio->seek_freq,
						radio->low->fm_state.freq, flag);
			}
		} else {
			if (radio->wrap_around &&
					(radio->seek_freq == radio->low->fm_state.freq)) {
				flag = fm_update_rx_status(radio, d_status);
				fm_update_if_count(radio);
				fm_update_sig_info(radio);
				fm_search_done(radio, flag);
			} else {
				radio->low->fm_tuner_state.hit_band_limit =
						fm_tuner_push_freq(
								radio,
								radio->low->fm_tuner_state.curr_search_down);
				/* disable audio out */
#if defined(CONFIG_RADIO_S610)
				fm_audio_control(radio, 0, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
				fm_audio_control(radio, 0, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */

				fm_set_freq(radio, radio->low->fm_state.freq, 1);

				/* enable audio out */
#if defined(CONFIG_RADIO_S610)
				fm_audio_control(radio, 1, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
				fm_audio_control(radio, 1, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */
				radio->tune_fniarg = 0;
				radio->dwork_tune_counter++;
				schedule_delayed_work(&radio->dwork_tune,
						msecs_to_jiffies(0));
			}
		}
	} else {
		radio->sig2_fniarg = 1;
		radio->dwork_sig2_counter++;
		schedule_delayed_work(&radio->dwork_sig2,
				msecs_to_jiffies(0));
	}
	/*	API_EXIT(radio);*/
}

static void fm_search_tuned(unsigned long data)
{
	struct s610_radio *radio = (void *) data;
	u16 count;

	API_ENTRY(radio);

	count = (fmspeedy_get_reg(FM_DEMOD_IF_COUNTER_INT_TIME) * 5) / 10;
	if (count > 20)
		print_ana_register();

	usleep_range(20 * 1000, 20 * 1000);
	fm_start_if_counter();
	fmspeedy_set_reg_field(FM_INT_CLEAR, 0, 1, 1); /* Clear Int. */
	usleep_range(10 * 1000, 10 * 1000);

	if (fm_check_rssi_level(radio->low->fm_state.rssi_limit_search)) {
		fm_search_check_signal1(radio, TRUE);
	} else {
		usleep_range(6 * 1000, 6 * 1000);

		fm_search_check_signal1(radio, FALSE);
	}

	API_EXIT(radio);
}

#ifdef USE_FILTER_SELECT_BY_FREQ
static const u32 filter_freq_very[MAX_FILTER_FREQ_NUM] = {
	87900, 88100, 95900, 96100, 103900, 104100
};

static bool is_freq_in_array(int freq)
{
	int i;

	for (i = 0; i < MAX_FILTER_FREQ_NUM; i++) {
		if (filter_freq_very[i] == freq)
			return TRUE;
	}
	return FALSE;
}
#endif /* USE_FILTER_SELECT_BY_FREQ */

#ifdef	RDS_POLLING_ENABLE
void fm_rds_periodic_cancel(unsigned long data)
{
	struct s610_radio *radio = (struct s610_radio *) data;

	cancel_delayed_work(&radio->dwork_rds_poll);
}

void fm_rds_periodic_update(unsigned long data)
{
	struct s610_radio *radio = (struct s610_radio *) data;

	radio->dwork_rds_counter++;
	schedule_delayed_work(&radio->dwork_rds_poll,
		msecs_to_jiffies(RDS_POLL_DELAY_MS));
}
#endif	/*RDS_POLLING_ENABLE*/

#ifdef	IDLE_POLLING_ENABLE
void fm_idle_periodic_cancel(unsigned long data)
{
	struct s610_radio *radio = (struct s610_radio *) data;

	cancel_delayed_work(&radio->dwork_idle_poll);
}

void fm_idle_periodic_update(unsigned long data)
{
	struct s610_radio *radio = (struct s610_radio *) data;

	radio->dwork_idle_counter++;
	schedule_delayed_work(&radio->dwork_idle_poll,
		msecs_to_jiffies(IDLE_TIME_MS));
}
#endif	/*IDLE_POLLING_ENABLE*/

static void fm_start_tune(struct s610_radio *radio, enum fm_tuner_state_low new_state)
{
	bool next = !!(radio->low->fm_state.tuner_mode & TUNER_MODE_MASK_NEXT);

	API_ENTRY(radio);

	switch (new_state) {
	case TUNER_NOTTUNED:
		break;
	case TUNER_IDLE:
		radio->low->fm_tuner_state.tune_done = TRUE;

#ifdef USE_IQ_IMBAL_SMOOTH
		hal_set_fm_image_trim_smooth_config_lock(1);
#endif /*USE_IQ_IMBAL_SMOOTH*/

#ifdef USE_FILTER_SELECT_BY_FREQ
		if (is_freq_in_array(radio->low->fm_state.freq))
			/* Set the filter to use very narrow band */
			fmspeedy_set_reg(FM_DEMOD_FILTER_SELECT, 0x0DB6);
		else
			/* Set the filter to use narrow band */
			fmspeedy_set_reg(FM_DEMOD_FILTER_SELECT, 0x0B6D);
#else
		fmspeedy_set_reg(FM_DEMOD_FILTER_SELECT, 0x0B6D);
#endif /* USE_FILTER_SELECT_BY_FREQ */

		if (!radio->low->fm_state.tuner_mode)
			radio->low->fm_state.mute_audio = 0;
		fm_tuner_control_mute(radio);

#ifdef MONO_SWITCH_INTERF
		fm_reset_force_mono_interf(radio);
#endif
		break;
	case TUNER_PRESET:
		fm_tuner_enable_rds(radio, FALSE);
		radio->low->fm_tuner_state.hit_band_limit = FALSE;
		if (next)
			radio->low->fm_tuner_state.hit_band_limit =
				fm_tuner_push_freq(
				radio,
				radio->low->fm_tuner_state.curr_search_down);

		/* disable audio out */
#if defined(CONFIG_RADIO_S610)
		fm_audio_control(radio, 0, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
		fm_audio_control(radio, 0, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */

		fm_set_freq(radio, radio->low->fm_state.freq, 1);

		/* enable audio out */
#if defined(CONFIG_RADIO_S610)
		fm_audio_control(radio, 1, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
		fm_audio_control(radio, 1, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */

		msleep(TUNE_TIME_SLOW_MS);
		fm_preset_tuned(radio);

		break;
	case TUNER_SEARCH:
		fm_tuner_enable_rds(radio, FALSE);
		fm_tuner_set_mute_audio(radio, TRUE);
		fm_set_rssi_thresh(radio, new_state);
		radio->low->fm_tuner_state.hit_band_limit = FALSE;

		if (next) {
			radio->low->fm_tuner_state.hit_band_limit =
				fm_tuner_push_freq(
				radio,
				radio->low->fm_tuner_state.curr_search_down);
		}

		/* disable audio out */
#if defined(CONFIG_RADIO_S610)
		fm_audio_control(radio, 0, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
		fm_audio_control(radio, 0, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */

		fm_set_freq(radio, radio->low->fm_state.freq, 1);

		/* enable audio out */
#if defined(CONFIG_RADIO_S610)
		fm_audio_control(radio, 1, 1, 0x100, 0x1A0);
#elif defined(CONFIG_RADIO_S621)
		fm_audio_control(radio, 1, 1, 0x100, 0x140);
#endif /* defined(CONFIG_RADIO_S610) */

		radio->tune_fniarg = 0;
		radio->dwork_tune_counter++;
		schedule_delayed_work(&radio->dwork_tune,
			msecs_to_jiffies(0));
		break;
	default:
		break;
	}

	API_EXIT(radio);

}

static void fm_tuner_change_state(struct s610_radio *radio,
		enum fm_tuner_state_low new_state)
{
	radio->low->fm_tuner_state.tuner_state = new_state;
	fm_update_tuner_mode(radio);

	switch (new_state) {
	case TUNER_OFF:
		break;
	case TUNER_NOTTUNED:
		radio->low->fm_tuner_state.tune_done = FALSE;
		fm_tuner_enable_rds(radio, FALSE);
		break;
	case TUNER_IDLE:
	case TUNER_PRESET:
	case TUNER_SEARCH:
		fm_start_tune(radio, new_state);
		break;
	}
}

static void fm_cancel_delayed_work(struct s610_radio *radio)
{
	cancel_delayed_work(&radio->dwork_sig2);
	cancel_delayed_work(&radio->dwork_tune);
}

static void fm_tuner_exit_state(struct s610_radio *radio)
{
	fm_cancel_delayed_work(radio);
#ifndef	RDS_POLLING_ENABLE
	fm_set_handler_if_count(NULL);
	fm_set_handler_audio_pause(NULL);
#endif	/*RDS_POLLING_ENABLE*/
	fm_set_rssi_thresh(radio, TUNER_IDLE);
}

void fm_set_tuner_mode(struct s610_radio *radio)
{
	u8 tm;
	enum fm_tuner_state_low new_state;

	API_ENTRY(radio);

	if (!radio->low->fm_state.fm_pwr_on) {
		fm_tuner_exit_state(radio);
		fm_tuner_change_state(radio, TUNER_OFF);
	} else {
		tm = radio->low->fm_state.tuner_mode & TUNER_MODE_MASK_TUN_MOD;
		new_state =
			radio->low->fm_tuner_state.tune_done ?
				TUNER_IDLE : TUNER_NOTTUNED;

		switch (tm) {
		case TUNER_MODE_PRESET:
			new_state = TUNER_PRESET;
			break;
		case TUNER_MODE_SEARCH:
			new_state = TUNER_SEARCH;
			break;
		case TUNER_MODE_NONE:
		default:
			break;
		}

		fm_tuner_exit_state(radio);
		fm_tuner_change_state(radio, new_state);
	}

	API_EXIT(radio);
}

static bool fm_tuner_on(struct s610_radio *radio)
{
	API_ENTRY(radio);

	if (!fm_radio_on(radio)) {
		radio->low->fm_state.fm_pwr_on =
		radio->low->fm_state.rds_pwr_on =
		FALSE;
		fm_tuner_exit_state(radio);
		fm_tuner_change_state(radio, TUNER_OFF);

		return FALSE;
	}

	API_EXIT(radio);

	return TRUE;
}

static void fm_tuner_off(struct s610_radio *radio)
{
	fm_radio_off(radio);
}

void fm_tuner_rds_on(struct s610_radio *radio)
{
	fm_rds_on(radio);

	if (radio->low->fm_state.rds_rx_enabled)
		fm_rds_enable(radio);
}

void fm_tuner_rds_off(struct s610_radio *radio)
{
	if (radio->low->fm_state.rds_rx_enabled)
		fm_rds_disable(radio);

	fm_rds_off(radio);
}

bool fm_tuner_set_power_state(struct s610_radio *radio, bool fm_on, bool rds_on)
{

	API_ENTRY(radio);

	if (fm_on && !radio->low->fm_state.fm_pwr_on) {
		fm_tuner_exit_state(radio);
		fm_tuner_change_state(radio, TUNER_NOTTUNED);
		fm_tuner_control_mute(radio);
	} else if (!fm_on && radio->low->fm_state.fm_pwr_on) {
		fm_tuner_exit_state(radio);
		fm_tuner_change_state(radio, TUNER_OFF);
	}

	if (fm_on && !radio->low->fm_state.fm_pwr_on) {
		if (!fm_tuner_on(radio))
			return FALSE;

		radio->low->fm_state.fm_pwr_on = TRUE;
	}

	if (rds_on && !radio->low->fm_state.rds_pwr_on) {
		if (radio->low->fm_state.fm_pwr_on) {
			fm_tuner_rds_on(radio);
			radio->low->fm_state.rds_pwr_on = TRUE;
		}
	} else if ((!rds_on || !fm_on) && radio->low->fm_state.rds_pwr_on) {
		fm_tuner_rds_off(radio);
		radio->low->fm_state.rds_pwr_on = FALSE;
	}

	if (!fm_on && radio->low->fm_state.fm_pwr_on) {
		fm_tuner_off(radio);
		radio->low->fm_state.fm_pwr_on = FALSE;
		fm_tuner_enable_rds(radio, FALSE);
	}

	API_EXIT(radio);

	return TRUE;
}

struct fm_conf_ini_values low_fm_conf_init = {
		.demod_conf_ini = 0x228C,
#if defined(CONFIG_SOC_EXYNOS7885)
		.rssi_adj_ini = 0x005A,
#elif defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS3830)
		.rssi_adj_ini = 0x006E,
#endif /* defined(CONFIG_SOC_EXYNOS7885) || defined(CONFIG_SOC_EXYNOS3830) */
		.soft_muffle_conf_ini = { 0x2516, 1, 1, 7 },
		.soft_mute_atten_max_ini = 0x0007,
		.stereo_thres_ini = 0x00C8,
		.narrow_thres_ini = 0x0074,
		.snr_adj_ini = 0x001C,
#if defined(CONFIG_RADIO_S610)
		.snr_smooth_conf_ini = 0x082F,
#elif defined(CONFIG_RADIO_S621)
		.snr_smooth_conf_ini = 0x00AF,
#endif /* defined(CONFIG_RADIO_S610) */
		.mute_coeffs_soft = 0x2516,
		.mute_coeffs_dis = 0x0000,
		.blend_coeffs_soft = 0x095A,
		.blend_coeffs_switch =	0x7D8C,
		.blend_coeffs_dis = 0x00FF,
		.rds_int_byte_count = RDS_MEM_MAX_THRESH,
		.search_conf = { 4500, 6000, 7000, 4000, 4800, 5600, FALSE},
#ifdef MONO_SWITCH_INTERF
		.interf_rssi = { -85, -75 },
		.interf_snr = { 20, 43 },
#endif
		.rds_error_limit = 3
};

struct fm_state_s low_fm_state_init = {
		.rds_rx_enabled = FALSE,
		.fm_pwr_on = FALSE,
		.rds_pwr_on = FALSE,
		.force_mono = FALSE,
		.use_switched_blend = FALSE,
		.use_soft_mute = TRUE,
		.mute_forced = FALSE,
		.mute_audio = FALSE,
		.search_down = FALSE,
		.use_rbds = FALSE,
		.save_eblks = FALSE,
		.last_status_blend_stereo = FALSE,
		.last_status_rds_sync = FALSE,
#ifdef MONO_SWITCH_INTERF
		.force_mono_interf = FALSE,
		.interf_checked = FALSE,
		.mono_switched_interf = FALSE,
		.mono_interf_reset_time = 0,
#endif
		.tuner_mode = 0,
		.status = 0,
		.rds_mem_thresh = 0,
		.rssi = 0,
		.band = 0,
		.last_ifc = 0,
		.snr = 0,
		.rssi_limit_normal = 0,
		.rssi_limit_search = 0,
		.freq = 0,
		.flags = 0,
		.rds_unsync_uncorr_weight = 10,
		.rds_unsync_blk_cnt = 20,
		.rds_unsync_bit_cnt = 48
};

struct fm_tuner_state_s low_fm_tuner_state_init = {
		.tuner_state = TUNER_OFF,
		.curr_search_down = FALSE,
		.hit_band_limit = FALSE,
		.tune_done = FALSE,
		.freq_step = 100,
		.band_limit_lo = 87500,
		.band_limit_hi = 108000
};

struct fm_band_s fm_bands_init[] = { { 87500, 108000 }, { 76000, 90000 }, {76000, 108000} };
u16 fm_freq_steps_init[] = { 50, 100, 200 };
#ifdef	USE_S612_DUAL_CLOCKING
u32 filter_freq_spur_case_1[] = {
#if defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630)
		87900, 88000, 88100, 95900, 96000, 96100, 99900, 100100 };
#elif defined(CONFIG_SOC_EXYNOS7885) || defined(CONFIG_SOC_EXYNOS3830)
		80000, 86400, 87900, 88000, 88100, 95900, 96000, 96100, 99900, 100100,
		89600, 91200, 92800, 94400, 97600, 99200, 100800, 102400, 105600, 107200 };
#endif /* defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS3830) */
#endif	/* USE_S612_DUAL_CLOCKING */

#ifdef USE_SPUR_CANCEL_TRF
u32 filter_freq_spur_select[] = {
		100000, 104000};
#endif /* USE_SPUR_CANCEL_TRF */

int init_low_struc(struct s610_radio *radio)
{
	memcpy(&radio->low->fm_config, &low_fm_conf_init,
			sizeof(struct fm_conf_ini_values));
	memcpy(&radio->low->fm_state, &low_fm_state_init, sizeof(struct fm_state_s));
	memcpy(&radio->low->fm_tuner_state, &low_fm_tuner_state_init,
			sizeof(struct fm_tuner_state_s));
	memcpy(&radio->low->fm_bands, &fm_bands_init,
		sizeof(struct fm_band_s) * (sizeof(fm_bands_init)/sizeof(struct fm_band_s)));

	memcpy(&radio->low->fm_freq_steps,
			&fm_freq_steps_init, sizeof(u16) * 3);
	if (radio->sw_mute_weak) {
		radio->low->fm_config.soft_muffle_conf_ini.muffle_coeffs = 0x1B16;
		radio->low->fm_config.mute_coeffs_soft = 0x1B16;
	}
#ifdef USE_SPUR_CANCEL
	if (radio->tc_on)
		memcpy(radio->low->fm_spur, fm_spur_init,
				sizeof(u32) * radio->tc_on);
#endif
#ifdef	USE_S612_DUAL_CLOCKING
	if (!radio->dual_clk_on) {
		memcpy(radio->low->fm_dual_clk, filter_freq_spur_case_1,
				sizeof(filter_freq_spur_case_1));
		fm_dual_clk_init = radio->low->fm_dual_clk;
		radio->dual_clk_on = sizeof(filter_freq_spur_case_1)/sizeof(u32);
	}
#endif	/* USE_S612_DUAL_CLOCKING */
#ifdef USE_SPUR_CANCEL_TRF
	if (!radio->trf_on) {
		memcpy(radio->low->fm_spur_trf, filter_freq_spur_select,
				sizeof(filter_freq_spur_select));
		fm_spur_trf_init = radio->low->fm_spur_trf;
		radio->trf_on = sizeof(filter_freq_spur_select)/sizeof(u32);
		radio->trf_spur = 1536;
	}
#endif /* USE_SPUR_CANCEL_TRF */

	return 0;
}
