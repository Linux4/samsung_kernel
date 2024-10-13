/*
 * sound/soc/codec/s5m3700x-jack.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include "s5m3700x.h"
#include "s5m3700x-jack.h"
#include "s5m3700x-register.h"
#include "s5m3700x-regmap.h"

static void s5m3700x_jackstate_set(struct s5m3700x_priv *s5m3700x, unsigned int change_state);


static int read_adc(struct s5m3700x_priv *s5m3700x, int type, int pin, int repeat)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	unsigned int value = 0, i = 0, v1 = 0, v2 = 0;

	mutex_lock(&jackdet->adc_lock);

	switch (pin) {
	case JACK_LDET_ADC:
			s5m3700x_update_bits(s5m3700x, S5M3700X_0E4_DCTR_FSM5, GPMUX_POLE_MASK, POLE_HPL_DET << GPMUX_POLE_SHIFT);
		break;
	case JACK_MDET_ADC:
			s5m3700x_update_bits(s5m3700x, S5M3700X_0E4_DCTR_FSM5, GPMUX_POLE_MASK, POLE_MIC_DET << GPMUX_POLE_SHIFT);
		break;
	case JACK_IMP_ADC:
		break;
	case JACK_WTP_LDET_ADC:
			s5m3700x_update_bits(s5m3700x, S5M3700X_0E4_DCTR_FSM5, GPMUX_WTP_MASK, POLE_HPL_DET << GPMUX_WTP_SHIFT);
		break;
	}

	switch (type) {
	case ADC_POLE:
		for (i = 0; i < repeat; i++) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_0E5_DCTR_FSM6, AP_REREAD_POLE_MASK, AP_REREAD_POLE_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_0E5_DCTR_FSM6, AP_REREAD_POLE_MASK, 0);
			jackdet->adc_re_read = 1;
		}
		msleep(100);
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F4_STATUS5, &v1);
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F7_STATUS8, &v2);

		v1 = v1 & 0x0F;
		value = (v1 << 8) + v2;
		break;
	case ADC_BTN:
		for (i = 0; i < repeat; i++) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_0E5_DCTR_FSM6, AP_REREAD_BTN_MASK, AP_REREAD_BTN_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_0E5_DCTR_FSM6, AP_REREAD_BTN_MASK, 0);
		}
		msleep(10);
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F5_STATUS6, &v1);
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F8_STATUS9, &v2);

		v1 = v1 & 0xF0;
		value = (v1 << 4) + v2;
		break;
	case ADC_IMP:
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F5_STATUS6, &v1);
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F9_STATUS10, &v2);

		v1 = v1 & 0x0F;
		value = (v1 << 8) + v2;
		break;
	case ADC_WTP:
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F2_STATUS3, &v1);
		s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F3_STATUS4, &v2);

		v1 = v1 & 0xF0;
		value = (v1 << 4) + v2;
		break;
	}

	mutex_unlock(&jackdet->adc_lock);
	dev_info(s5m3700x->dev, "%s called. ADC Pin: %d, Value: %d\n", __func__, pin, value);
	return value;
}

static int calculate_imp(struct s5m3700x_jack *jackdet, unsigned int d_out)
{
	long long p1 = jackdet->imp_p[0];
	long long p2 = jackdet->imp_p[1];
	long long p3 = jackdet->imp_p[2];
	long long c1 = 99, c2 = 79, c3 = 670;
	long long temp1, temp2, temp3, result;

	if ((p1 == 0) || (p2 == 0) || (p3 == 0))
		return 32;

	temp1 = p1 * ((c3 * p1) + (long long)1024) *
		((c2 * (long long)d_out) - (c2 * p3) - (c1 * p2) - (c2 * p2));
	temp2 = p2 * ((c1 * p1) + (c2 * p1) + (c3 * p1) + (long long)1024);
	temp3 = (d_out - p3) * ((c3 * p1) + (c2 * p1) + (long long)1024);
	result = temp1 / ((long long)128 * (temp2 - temp3));

	dev_info(jackdet->p_s5m3700x->dev,
			"%s called. tp1: %lld, tp2: %lld, tp3: %lld\n",
			__func__, temp1, temp2, temp3);

	return result;
}

static int s5m3700x_get_imp(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	unsigned int imp_adc;
	int cal;

	imp_adc = read_adc(s5m3700x, ADC_IMP, JACK_IMP_ADC, 0);
	cal = calculate_imp(jackdet, imp_adc);

	dev_info(s5m3700x->dev, "%s called. impedance: %d\n", __func__, cal);

	return cal;
}

/*
 * Workqueue handle
 */

//left impedance check for water decision
static void s5m3700x_ldet_chk_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, ldet_chk_work.work);
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct device *dev = s5m3700x->dev;

	unsigned int decision_state = JACK_WTP_JO;
	int ldet_adc;

	dev_info(dev, "%s called, cur_jack_state: %s.\n",
			__func__, s5m3700x_return_status_name(jackdet->cur_jack_state));

	/* Pole value decision */
	if (jackdet->cur_jack_state & JACK_WTP_DEC) {
		/* Read ldet adc value for Water Protection */
		// need to change JACK_LDET_ADC to WTP ADC
		ldet_adc = read_adc(s5m3700x, ADC_WTP, JACK_WTP_LDET_ADC, 1);

		if (ldet_adc < jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_LOW_RANGE]) {
			decision_state = JACK_WTP_CJI;
		} else if ((ldet_adc >= jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_LOW_RANGE])
					&& (ldet_adc < jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_MID_RANGE])) {
			decision_state = JACK_WTP_ETC;
		} else if ((ldet_adc >= jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_MID_RANGE])
					&& (ldet_adc < jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_HIGH_RANGE])) {
			decision_state = JACK_WTP_POLL;
		} else {
			decision_state = JACK_WTP_JO;
		}
		s5m3700x_jackstate_set(s5m3700x, decision_state);
	} else {
		dev_info(dev, "%s called, Jack state is not JACK_WTP_DEC\n", __func__);
	}

	jack_wake_unlock(jackdet->jack_wakeup);
}

static void s5m3700x_jack_det_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, jack_det_work.work);
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct device *dev = s5m3700x->dev;
	int jack_state = JACK_4POLE;
	int mdet_adc = 0, ldet_adc = 0;

	mutex_lock(&jackdet->jack_det_lock);
	dev_info(dev, "%s called, cur_jack_state: %s.\n",
			__func__, s5m3700x_return_status_name(jackdet->cur_jack_state));

	/* Pole value decision */
	if (jackdet->cur_jack_state & JACK_POLE_DEC) {
		/* Read mdet adc value for detect mic */
		mdet_adc = read_adc(s5m3700x, ADC_POLE, JACK_MDET_ADC, 0);

		if (mdet_adc < jackdet->mic_adc_range[0])
			jack_state = JACK_3POLE;
		else if ((mdet_adc >= jackdet->mic_adc_range[0]) &&
			(mdet_adc < jackdet->mic_adc_range[1]))
			jack_state = JACK_4POLE;
		else
			jack_state = JACK_AUX;

		/* Read ldet adc value for OMTP Jack */
		if (jackdet->omtp_range != S5M3700X_OMTP_ADC_DEFAULT) {
			ldet_adc = read_adc(s5m3700x, ADC_POLE, JACK_LDET_ADC, 1);
			if (ldet_adc > jackdet->omtp_range)
				jack_state = JACK_OMTP;
		} else {
			ldet_adc = -EINVAL;
		}

		/* Report jack type */
		s5m3700x_jackstate_set(s5m3700x, jack_state);
		s5m3700x_report_jack_type(s5m3700x, S5M3700X_JACK_MASK, 1);

		dev_info(dev, "%s ldet: %d, mdet: %d, Jack: %s, Pole: %s\n", __func__,
				ldet_adc, mdet_adc,
				(jackdet->cur_jack_state & JACK_OMTP) ? "OMTP" : "CTIA",
				s5m3700x_print_jack_type_state(jackdet->cur_jack_state));
	} else if (jackdet->cur_jack_state & JACK_WTP_ETC) {
		/*
		 * Raise Aux state by WTP Process
		 */
		jack_state = JACK_AUX;
		/* Report jack type */
		s5m3700x_jackstate_set(s5m3700x, jack_state);
		s5m3700x_report_jack_type(s5m3700x, S5M3700X_JACK_MASK, 1);
		dev_info(dev, "%s WTP decision Jack: CTIA, Pole: AUX\n", __func__);
	} else if (jackdet->cur_jack_state & JACK_IN) {
		/*
		 * Already Jack-in, Re-Check the Pole
		 */
		dev_dbg(dev, "%s called, Re-check the Pole\n", __func__);
		s5m3700x_report_jack_type(s5m3700x, S5M3700X_JACK_MASK, 1);
	} else {
		if (jackdet->cur_jack_state & JACK_OUT)
			dev_info(dev, "%s called, JACK_OUT State.\n", __func__);
		else
			dev_info(dev, "%s called, Unsupported state\n", __func__);
		s5m3700x_report_jack_type(s5m3700x, S5M3700X_JACK_MASK, 0);
	}

	dev_info(dev, "%s called, Jack %s, Mic %s\n", __func__,
			(jackdet->cur_jack_state & JACK_IN) ?	"inserted" : "removed",
			(jackdet->cur_jack_state & JACK_4POLE) ? "inserted" : "removed");

	mutex_unlock(&jackdet->jack_det_lock);
	jack_wake_unlock(jackdet->jack_wakeup);
}

static void s5m3700x_button_release_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, btn_release_work.work);
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct device *dev = s5m3700x->dev;

	dev_info(dev, "%s called\n", __func__);

	/* Check error status */
	if (!s5m3700x_button_error_check(s5m3700x, BUTTON_RELEASE)) {
		jack_wake_unlock(jackdet->jack_wakeup);
		return;
	}

	s5m3700x_report_button_type(s5m3700x, S5M3700X_BUTTON_MASK, BUTTON_RELEASE, 0);

	jack_wake_unlock(jackdet->jack_wakeup);
}

static void s5m3700x_button_press_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, btn_press_work.work);
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct device *dev = s5m3700x->dev;
	int btn_adc = 0;

	dev_info(dev, "%s called\n", __func__);

	/* Check error status */
	if (!s5m3700x_button_error_check(s5m3700x, BUTTON_PRESS)) {
		jack_wake_unlock(jackdet->jack_wakeup);
		return;
	}

	btn_adc = read_adc(s5m3700x, ADC_BTN, JACK_MDET_ADC, 0);

	s5m3700x_report_button_type(s5m3700x, S5M3700X_BUTTON_MASK, BUTTON_PRESS, btn_adc);

	jack_wake_unlock(jackdet->jack_wakeup);
}

/*
 * Jack Detection Process Routine Start
 * handler -> jackstate_set -> jackstate_register -> workqueue (if neeeded)
 */

//jackstate_register
static void s5m3700x_jackout_register(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC4))
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);

	jackdet->adc_re_read = 0;

	s5m3700x_write(s5m3700x, S5M3700X_03A_AD_VOL, 0x00);
	s5m3700x_write(s5m3700x, S5M3700X_0E0_DCTR_FSM1, 0x0A);
	s5m3700x_write(s5m3700x, S5M3700X_0E3_DCTR_FSM4, 0x00);

	/* ODSEL5 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0B4_ODSEL5,
			EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK);
}

static void s5m3700x_jackcmp_register(struct s5m3700x_priv *s5m3700x)
{
	return;
}

static void s5m3700x_jack_wtp_dec_register(struct s5m3700x_priv *s5m3700x)
{
	return;
}

static void s5m3700x_jack_wtp_cji_register(struct s5m3700x_priv *s5m3700x)
{
	/* Completely Jack In */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_CJI_MASK, AP_CJI_MASK);
	s5m3700x_usleep(100);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_CJI_MASK, 0x0);
}

static void s5m3700x_jack_wtp_etc_register(struct s5m3700x_priv *s5m3700x)
{
	/* Slightly Jack In */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_ETC_MASK, AP_ETC_MASK);
	s5m3700x_usleep(100);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_ETC_MASK, 0x0);
}

static void s5m3700x_jack_wtp_poll_register(struct s5m3700x_priv *s5m3700x)
{
	/* Water In */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_POLLING_MASK, AP_POLLING_MASK);
	s5m3700x_usleep(100);
}

static void s5m3700x_jack_wtp_jo_register(struct s5m3700x_priv *s5m3700x)
{
	/* Jack Out*/
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_JO_MASK, AP_JO_MASK);
	s5m3700x_usleep(100);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_JO_MASK, 0x0);
}

static void s5m3700x_jack_wtp_jio_register(struct s5m3700x_priv *s5m3700x)
{
	/* Polling off */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E0_DCTR_FSM1, AP_POLLING_MASK, 0x00);
	s5m3700x_usleep(100);
	/* GPADC Re-read */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E5_DCTR_FSM6, AP_REREAD_WTP_MASK, AP_REREAD_WTP_MASK);
	s5m3700x_usleep(100);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E5_DCTR_FSM6, AP_REREAD_WTP_MASK, 0x00);
}

static void s5m3700x_jack_imp_register(struct s5m3700x_priv *s5m3700x)
{
	unsigned int imp_value = s5m3700x_get_imp(s5m3700x);

	if (imp_value < 10)
		s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 2 << IMP_VAL_SHIFT);
	else if (imp_value >= 10 && imp_value < 32)
		s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 1 << IMP_VAL_SHIFT);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 0 << IMP_VAL_SHIFT);
}

static void s5m3700x_jack_pole_dec_register(struct s5m3700x_priv *s5m3700x)
{
	return;
}

static void s5m3700x_jack_4pole_register(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E3_DCTR_FSM4,
			POLE_VALUE_MASK, POLE_VALUE_4POLE << POLE_VALUE_SHIFT);

	s5m3700x_write(s5m3700x, S5M3700X_03A_AD_VOL, 0x07);
	/* ODSEL5 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0B4_ODSEL5,
			EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
}

static void s5m3700x_jack_3pole_register(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E3_DCTR_FSM4,
			POLE_VALUE_MASK, POLE_VALUE_3POLE << POLE_VALUE_SHIFT);
	/* ODSEL5 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0B4_ODSEL5,
			EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
}

static void s5m3700x_jack_aux_register(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E3_DCTR_FSM4,
			POLE_VALUE_MASK, POLE_VALUE_AUX << POLE_VALUE_SHIFT);
	/* ODSEL5 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0B4_ODSEL5,
			EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
}

static void s5m3700x_jack_omtp_register(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E3_DCTR_FSM4,
			POLE_VALUE_MASK, POLE_VALUE_OMTP << POLE_VALUE_SHIFT);
	/* ODSEL5 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0B4_ODSEL5,
			EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
}

static bool s5m3700x_jackstate_register(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	unsigned int cur_jack, prv_jack;

	cur_jack = jackdet->cur_jack_state;
	prv_jack = jackdet->prv_jack_state;

	if (!s5m3700x_check_jack_state_sequence(s5m3700x))
		return false;

	s5m3700x_print_status_change(dev, prv_jack, cur_jack);
	switch (cur_jack) {
	case JACK_OUT:
		s5m3700x_jackout_register(s5m3700x);
		break;
	case JACK_CMP:
		s5m3700x_jackcmp_register(s5m3700x);
		break;
	case JACK_WTP_DEC:
		s5m3700x_jack_wtp_dec_register(s5m3700x);
		break;
	case JACK_WTP_CJI:
		s5m3700x_jack_wtp_cji_register(s5m3700x);
		break;
	case JACK_WTP_ETC:
		s5m3700x_jack_wtp_etc_register(s5m3700x);
		break;
	case JACK_WTP_POLL:
		s5m3700x_jack_wtp_poll_register(s5m3700x);
		break;
	case JACK_WTP_JO:
		s5m3700x_jack_wtp_jo_register(s5m3700x);
		break;
	case JACK_WTP_JIO:
		s5m3700x_jack_wtp_jio_register(s5m3700x);
		break;
	case JACK_IMP:
		s5m3700x_jack_imp_register(s5m3700x);
		break;
	case JACK_POLE_DEC:
		s5m3700x_jack_pole_dec_register(s5m3700x);
		break;
	case JACK_4POLE:
		s5m3700x_jack_4pole_register(s5m3700x);
		break;
	case JACK_3POLE:
		s5m3700x_jack_3pole_register(s5m3700x);
		break;
	case JACK_AUX:
		s5m3700x_jack_aux_register(s5m3700x);
		break;
	case JACK_OMTP:
		s5m3700x_jack_omtp_register(s5m3700x);
		break;
	}
	return true;
}

//jackstate_set
static void s5m3700x_jackstate_set(struct s5m3700x_priv *s5m3700x,
		unsigned int change_state)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	int ret = 0;

	/* Save privious jack state */
	jackdet->prv_jack_state = jackdet->cur_jack_state;

	/* Update current jack state */
	jackdet->cur_jack_state = change_state;

	if (jackdet->prv_jack_state != jackdet->cur_jack_state) {
		dev_info(dev, "%s called, Prv: %s, Cur: %s\n",  __func__,
				s5m3700x_return_status_name(jackdet->prv_jack_state),
				s5m3700x_return_status_name(jackdet->cur_jack_state));

		/* Set jack register */
		ret = s5m3700x_jackstate_register(s5m3700x);
		dev_info(dev, "Jack register write %s.\n", ret ? "complete" : "incomplete");
	} else {
		dev_info(dev, "Prv_jack_state and Cur_jack_state are same (%s).\n",
					s5m3700x_return_status_name(jackdet->prv_jack_state));
	}
}

// jack out handler
static void s5m3700x_st_jackout_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	s5m3700x_jackstate_set(s5m3700x, JACK_OUT);

	cancel_delayed_work(&jackdet->ldet_chk_work);
	cancel_delayed_work(&jackdet->jack_det_work);
	queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
			msecs_to_jiffies(0));

	queue_delayed_work(jackdet->btn_release_wq, &jackdet->btn_release_work,
			msecs_to_jiffies(BTN_ADC_DELAY));
}

// completely jack in handler
static void s5m3700x_st_cmp_jack_in_handler(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_jackstate_set(s5m3700x, JACK_CMP);
}

// Water Decision handler
static void s5m3700x_st_wtjack_dec_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	if (jackdet->cur_jack_state == JACK_WTP_DEC) {
		dev_info(s5m3700x->dev, "[IRQ] %s IRQ_ST_WTJACK_DEC is already on going : %d\n", __func__, __LINE__);
		return;
	}

	s5m3700x_jackstate_set(s5m3700x, JACK_WTP_DEC);

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	/* run ldet adc workqueue */
	cancel_delayed_work(&jackdet->ldet_chk_work);
	queue_delayed_work(jackdet->ldet_chk_wq, &jackdet->ldet_chk_work,
			msecs_to_jiffies(LDET_CHK_DELAY));
}

// Water In handler
static void s5m3700x_st_wtjack_in_handler(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_jackstate_set(s5m3700x, JACK_WTP_JIO);
}

// Water Out handler
static void s5m3700x_st_wtjack_out_handler(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_jackstate_set(s5m3700x, JACK_WTP_JIO);
}

// Read HPZ_DET Impedance
static void s5m3700x_st_imp_chk_handler(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_jackstate_set(s5m3700x, JACK_IMP);
}

// Read MIC Impedance for pole decision
// Read Left Impedance for OMTP decision
// report jack type
static void s5m3700x_st_jackdet_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	if (jackdet->adc_re_read == 1) {
		jackdet->adc_re_read = 0;
		return;
	}
	s5m3700x_jackstate_set(s5m3700x, JACK_POLE_DEC);

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	cancel_delayed_work(&jackdet->jack_det_work);
	queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
			msecs_to_jiffies(JACK_MDET_DELAY));
}

// call jack_det_work for report aux from the result of water dicision
// Pole decision again for double check aux and report state
static void s5m3700x_st_wtjack_etc_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	cancel_delayed_work(&jackdet->jack_det_work);
	queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
			msecs_to_jiffies(JACK_MDET_DELAY));
}

static void s5m3700x_btn_release_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	queue_delayed_work(jackdet->btn_release_wq, &jackdet->btn_release_work,
			msecs_to_jiffies(BTN_ADC_DELAY));
}

static void s5m3700x_btn_press_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	queue_delayed_work(jackdet->btn_press_wq, &jackdet->btn_press_work,
			msecs_to_jiffies(BTN_ADC_DELAY));
}

static int s5m3700x_earjack_handler(struct s5m3700x_priv *s5m3700x, unsigned int event)
{
	struct device *dev = s5m3700x->dev;
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	int irq_type = s5m3700x_get_irq_type(s5m3700x, event);

	mutex_lock(&jackdet->key_lock);

	switch (irq_type) {
	case IRQ_ST_JACKOUT:
		dev_info(dev, "[IRQ] %s Jack out interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_jackout_handler(s5m3700x);
		break;
	case IRQ_ST_CMP_JACK_IN:
		dev_info(dev, "[IRQ] %s Completely Jack in interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_cmp_jack_in_handler(s5m3700x);
		break;
	case IRQ_ST_WTJACK_DEC:
		dev_info(dev, "[IRQ] %s Water Protection Decision interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_wtjack_dec_handler(s5m3700x);
		break;
	case IRQ_ST_WTJACK_IN:
		dev_info(dev, "[IRQ] %s IRQ_ST_WTJACK_IN, line: %d\n", __func__, __LINE__);
		s5m3700x_st_wtjack_in_handler(s5m3700x);
		break;
	case IRQ_ST_WTJACK_OUT:
		dev_info(dev, "[IRQ] %s IRQ_ST_WTJACK_OUT, line: %d\n", __func__, __LINE__);
		s5m3700x_st_wtjack_out_handler(s5m3700x);
		break;
	case IRQ_ST_IMP_CHK:
		dev_info(dev, "[IRQ] %s IMP interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_imp_chk_handler(s5m3700x);
		break;
	case IRQ_ST_JACKDET:
		dev_info(dev, "[IRQ] %s Jack det interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_jackdet_handler(s5m3700x);
		break;
	case IRQ_ST_WTJACK_ETC:
		dev_info(dev, "[IRQ] %s State WTP Aux interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_wtjack_etc_handler(s5m3700x);
		break;
	case IRQ_ST_BTN_RELEASE:
		dev_info(dev, "[IRQ] %s Button Release interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_btn_release_handler(s5m3700x);
		break;
	case IRQ_ST_BTN_PRESS:
		dev_info(dev, "[IRQ] %s Button Press interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_btn_press_handler(s5m3700x);
		break;
	default:
		dev_info(dev, "[IRQ] %s IRQ return type skip, line %d\n", __func__, __LINE__);
		break;
	}

	mutex_unlock(&jackdet->key_lock);

	return IRQ_HANDLED;
}

/*
 * Jack Detection Process Routine End
 */


static void s5m3700x_jack_parse_dt(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_common_parse_dt(s5m3700x);
}

/* Register workqueue */
static int s5m3700x_register_jack_workqueue(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* Initiallize workqueue for jack detect handling */
	INIT_DELAYED_WORK(&jackdet->jack_det_work, s5m3700x_jack_det_work);
	jackdet->jack_det_wq = create_singlethread_workqueue("jack_det_wq");
	if (jackdet->jack_det_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create jack_det_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for ldet detect handling */
	INIT_DELAYED_WORK(&jackdet->ldet_chk_work, s5m3700x_ldet_chk_work);
	jackdet->ldet_chk_wq = create_singlethread_workqueue("ldet_chk_wq");
	if (jackdet->ldet_chk_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create ldet_chk_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for button press, release */
	INIT_DELAYED_WORK(&jackdet->btn_release_work, s5m3700x_button_release_work);
	jackdet->btn_release_wq = create_singlethread_workqueue("btn_release_wq");
	if (jackdet->btn_release_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create btn_release_wq\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&jackdet->btn_press_work, s5m3700x_button_press_work);
	jackdet->btn_press_wq = create_singlethread_workqueue("btn_press_wq");
	if (jackdet->btn_press_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create btn_press_wq\n");
		return -ENOMEM;
	}
	return 0;
}

/* apply register initialize by patch
 * patch is only updated hw register, so cache need to be updated.
 */
static int s5m3700x_regmap_jack_register_patch(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;
	int ret = 0;

	/* jack register patch */
	ret = regmap_register_patch(s5m3700x->regmap, s5m3700x_jack_patch,
				ARRAY_SIZE(s5m3700x_jack_patch));
	if (ret < 0) {
		dev_err(dev, "Failed to apply s5m3700x_jack_patch %d\n", ret);
		return ret;
	}

	/* update reg_defaults with registered patch */
	s5m3700x_update_reg_defaults(s5m3700x_jack_patch, ARRAY_SIZE(s5m3700x_jack_patch));
	return ret;
}

static void s5m3700x_jack_register_initialize(struct s5m3700x_priv *s5m3700x)
{
	/* update initial registers on cache and hw registers */
	s5m3700x_regmap_jack_register_patch(s5m3700x);

	/* reinit regmap cache because reg_defaults are updated*/
	s5m3700x_regmap_reinit_cache(s5m3700x);
}

/*
 * s5m3700x_jack_notifier_handler() - Codec IRQ Handler
 *
 * Desc: Set codec register according to codec IRQ.
 */
static int s5m3700x_jack_notifier_handler(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct s5m3700x_priv *s5m3700x = data;

	dev_info(s5m3700x->dev, "%s called.\n", __func__);

	/* read irq registgers */
	s5m3700x_read_irq(s5m3700x);

	s5m3700x_earjack_handler(s5m3700x, event);

	return IRQ_HANDLED;
}

/* IRQ Process Thread Routine */
irqreturn_t s5m3700x_jack_irq_thread(int irq, void *irq_data)
{
	struct s5m3700x_jack *jackdet = irq_data;
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;

	dev_info(s5m3700x->dev, "%s called.\n", __func__);

	/* call notifier handler */
	s5m3700x_call_notifier(s5m3700x, JACK_HANDLER_EVENT_NONE);

	return IRQ_HANDLED;
}

/*
 * s5m3700x_jack_probe() - Initialize variable related jack
 *
 * @codec: SoC audio codec device
 *
 * Desc: This function is called by s5m3700x_component_probe. For separate codec
 * and jack code, this function called from codec driver. This function initialize
 * jack variable, workqueue, mutex, and wakelock.
 */
int s5m3700x_jack_probe(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet;
	int ret = 0;

	dev_info(s5m3700x->dev, "Codec Jack Probe: (%s)\n", __func__);

	jackdet = kzalloc(sizeof(struct s5m3700x_jack), GFP_KERNEL);
	if (jackdet == NULL)
		return -ENOMEM;

	/* initialize variable */
	s5m3700x_jack_variable_initialize(s5m3700x, jackdet);

	/* initialize variable for local value */
	jackdet->prv_jack_state = JACK_OUT;
	jackdet->cur_jack_state = JACK_OUT;

	/* Device Tree for jack */
	s5m3700x_jack_parse_dt(s5m3700x);

	/* Register workqueue */
	s5m3700x_register_jack_workqueue(s5m3700x);

	/* Register jack */
	s5m3700x_register_jack(codec, s5m3700x, S5M3700X_JACK_MASK);

	/* Register Button */
	s5m3700x_register_button(codec, s5m3700x, S5M3700X_BUTTON_MASK);

	/* s5m3700x_resume */
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3700x->dev))
		pm_runtime_get_sync(s5m3700x->dev);
#endif

	/* register initialize */
	s5m3700x_jack_register_initialize(s5m3700x);

	/* restore hp_trim */
	s5m3700x_hp_trim_initialize(s5m3700x);

	/* Sync Up WTP LDET threshold with HMU */
	s5m3700x_wtp_threshold_initialize(s5m3700x);

	/* Configure mic bias voltage */
	s5m3700x_configure_mic_bias(s5m3700x);

	/* Jack OTP Read */
	s5m3700x_read_imp_otp(s5m3700x);

	/* Register Jack controls */
	s5m3700x_register_jack_controls(codec, s5m3700x);

	/* Register jack detection notifier */
	s5m3700x_register_notifier(s5m3700x, s5m3700x_jack_notifier_handler);

	/* Request Jack IRQ */
	s5m3700x_request_threaded_irq(s5m3700x, s5m3700x_jack_irq_thread);

	/* s5m3700x_suspend */
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3700x->dev))
		pm_runtime_put_sync(s5m3700x->dev);
#endif
	jackdet->is_probe_done = true;
	return ret;
}

int s5m3700x_jack_remove(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	dev_info(codec->dev, "(*) %s called\n", __func__);

	destroy_workqueue(jackdet->ldet_chk_wq);
	destroy_workqueue(jackdet->jack_det_wq);
	destroy_workqueue(jackdet->btn_press_wq);
	destroy_workqueue(jackdet->btn_release_wq);

	s5m3700x_jack_register_exit(codec);

	/* Unregister ADC pin */
	kfree(jackdet);

	return 0;
}
