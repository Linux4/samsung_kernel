/*
 * sound/soc/codec/s5m3700x-usbjack.c
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

static struct s5m3700x_jack *p_s5m3700x_jack;

#if IS_ENABLED(CONFIG_USB_SW_MAX20328B)
extern max20328_switch_mode_event(enum max_function event);
#endif

static void s5m3700x_usbjackstate_set(struct s5m3700x_priv *s5m3700x, unsigned int change_state);

bool s5m3700x_is_probe_done(void)
{
	if (p_s5m3700x_jack == NULL)
		return false;
	return p_s5m3700x_jack->is_probe_done;
}
EXPORT_SYMBOL_GPL(s5m3700x_is_probe_done);

void micbias_switch_unlock(struct s5m3700x_jack *jackdet, bool on)
{
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct device *dev = jackdet->p_s5m3700x->dev;
	bool skip_ops = true;

	mutex_lock(&s5m3700x->hp_mbias_lock);

	if (on && !s5m3700x->hp_bias_cnt++) {
		skip_ops = false;
	} else if (!on && !--s5m3700x->hp_bias_cnt) {
		skip_ops = false;
	}

	if (s5m3700x->hp_bias_cnt < 0) {
		s5m3700x->hp_bias_cnt = 0;
		dev_info(dev, "%s, micbias operation is not match\n", __func__);
	}

	dev_info(dev, "%s called, micbias turn %s, cnt %d\n",
			__func__, on ? "On" : "Off", s5m3700x->hp_bias_cnt);

	if (skip_ops) {
		mutex_unlock(&s5m3700x->hp_mbias_lock);
		return;
	}

	if (on) {
		/* MICBIAS2 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB2_MASK, APW_MCB2_MASK);
		msleep(50);
	} else {
		/* MICBIAS2 APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB2_MASK, 0);
	}

	mutex_unlock(&s5m3700x->hp_mbias_lock);
}

void s5m3700x_usb_micbias(bool on)
{
	struct s5m3700x_jack *jackdet = NULL;
	struct s5m3700x_priv *s5m3700x = NULL;

	if (p_s5m3700x_jack != NULL) {
		jackdet = p_s5m3700x_jack;
		s5m3700x = jackdet->p_s5m3700x;

		dev_info(s5m3700x->dev, "%s called. Mic Bias: %s\n", __func__, on ? "On" : "Off");
		micbias_switch_unlock(jackdet, on);
	}
}
EXPORT_SYMBOL_GPL(s5m3700x_usb_micbias);

static int read_manual_adc(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	unsigned int value, v1, v2;

	mutex_lock(&jackdet->adc_lock);

	/* Enable the Testmode */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FE_DCTR_GP1, T_CNT_ADC_DIV_MASK, T_CNT_ADC_DIV_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FF_DCTR_GP2, T_ADC_STARTTIME_MASK, T_ADC_STARTTIME_MASK);

	/* ADC Divide Count Test Mode in Pole */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FE_DCTR_GP1,
			CNT_ADC_DIV_MASK, CNT_ADC_DIV_32 << CNT_ADC_DIV_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FF_DCTR_GP2,
			ADC_STARTTIME_MASK, ADC_STARTTIME_10 << ADC_STARTTIME_SHIFT);

	/* ADC Start */
	s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0x8C);
	s5m3700x_usleep(500);
	s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0xCC);

	msleep(80);

	s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0FA_STATUS11, &v1);
	s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0FB_STATUS12, &v2);

	v1 = v1 & 0xF0;
	value = (v1 << 4) + v2;

	/* ADC STOP */
	s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0x8C);
	s5m3700x_usleep(500);
	s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0x0C);

	/* Disable the Testmode */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FE_DCTR_GP1, T_CNT_ADC_DIV_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FF_DCTR_GP2, T_ADC_STARTTIME_MASK, 0);

	mutex_unlock(&jackdet->adc_lock);
	dev_info(s5m3700x->dev, "%s called. Value: %d\n", __func__, value);

	return value;
}

static int read_manual_adc_switch(struct s5m3700x_priv *s5m3700x)
{
	unsigned int value = 0;

	value = read_manual_adc(s5m3700x);

	return value;
}

static int read_usb_adc(struct s5m3700x_priv *s5m3700x, int type, int pin, int repeat)
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
		s5m3700x_update_bits(s5m3700x, S5M3700X_0E7_DCTR_IMP1, CNT_IMP_OPT1_MASK, 0);
		s5m3700x_update_bits(s5m3700x, S5M3700X_0E9_DCTR_IMP3, CNT_IMP_OPT5_MASK, 1 << CNT_IMP_OPT5_SHIFT);
		s5m3700x_update_bits(s5m3700x, S5M3700X_0EA_DCTR_IMP4, CNT_IMP_OPT7_MASK, 0);
		s5m3700x_update_bits(s5m3700x, S5M3700X_0EB_DCTR_IMP5, CNT_IMP_OPT8_MASK, 0);
		s5m3700x_update_bits(s5m3700x, S5M3700X_0EC_DCTR_IMP6, CNT_IMP_OPT10_MASK | CNT_IMP_OPT11_MASK, 0);

		/* HP PULLDN */
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_HP_PDN_MASK, 0);
		s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0x04);

		/* TEST Mode Enable */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0EC_DCTR_IMP6, T_IMP_SEQ_MASK, T_IMP_SEQ_MASK);

		/* It need for measuring impedance */
		msleep(400);
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
		msleep(30);

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

		s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0x0C);
		/* HP PULLDN */
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_HP_PDN_MASK, EN_HP_PDN_MASK);
		break;
	}

	mutex_unlock(&jackdet->adc_lock);
	dev_info(s5m3700x->dev, "%s called. ADC Pin: %d, Value: %d\n",
			__func__, pin, value);
	return value;
}

static int calculate_usb_imp(struct s5m3700x_priv *s5m3700x, unsigned int d_out)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
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

int s5m3700x_usbjack_get_imp(void)
{
	struct s5m3700x_jack *jackdet = NULL;
	struct s5m3700x_priv *s5m3700x = NULL;
	unsigned int imp_adc = 0;
	int cal = 0;

	if (p_s5m3700x_jack != NULL) {
		jackdet = p_s5m3700x_jack;
		s5m3700x = jackdet->p_s5m3700x;

		imp_adc = read_usb_adc(s5m3700x, ADC_IMP, JACK_IMP_ADC, 0);

		cal = calculate_usb_imp(s5m3700x, imp_adc);

		dev_info(s5m3700x->dev, "%s called. impedance: %d\n", __func__, cal);
	}

	return cal;
}
EXPORT_SYMBOL_GPL(s5m3700x_usbjack_get_imp);

int s5m3700x_return_usbjack_imp_range(void)
{
	struct s5m3700x_jack *jackdet = NULL;
	struct s5m3700x_priv *s5m3700x = NULL;
	int vimp = 0;

	if (p_s5m3700x_jack != NULL) {
		jackdet = p_s5m3700x_jack;
		s5m3700x = jackdet->p_s5m3700x;
		vimp = s5m3700x_usbjack_get_imp();

		dev_info(jackdet->p_s5m3700x->dev,
				"%s called. imp: %d, r1: %d, r2: %d, r3: %d, r4: %d, r5: %d\n",
				__func__, vimp, jackdet->jack_imp_range[0],
				jackdet->jack_imp_range[1], jackdet->jack_imp_range[2],
				jackdet->jack_imp_range[3], jackdet->jack_imp_range[4]);

		if (vimp < 10)
			s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 2 << IMP_VAL_SHIFT);
		else if (vimp >= 10 && vimp < 32)
			s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 1 << IMP_VAL_SHIFT);
		else
			s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 0 << IMP_VAL_SHIFT);

		/* Need to calibration */
		if (vimp >= jackdet->jack_imp_range[0]
				&& vimp < jackdet->jack_imp_range[1]) {
			/* 4 ohm */
			return JACK_IMP_VAL_1;
		} else if (vimp >= jackdet->jack_imp_range[1]
				&& vimp < jackdet->jack_imp_range[2]) {
			/* 24 ohm */
			return JACK_IMP_VAL_2;
		} else if (vimp >= jackdet->jack_imp_range[2]
				&& vimp < jackdet->jack_imp_range[3]) {
			/* 40 ohm */
			return JACK_IMP_VAL_3;
		} else if (vimp >= jackdet->jack_imp_range[3]
				&& vimp < jackdet->jack_imp_range[4]) {
			/* 40 ~ 100 ohm */
			return JACK_IMP_VAL_4;
		} else if (vimp >= jackdet->jack_imp_range[4]) {
			/* 100 ohm ~ */
			return JACK_IMP_VAL_5;
		} else {
			return -1;
		}
	}
	return -1;
}
EXPORT_SYMBOL_GPL(s5m3700x_return_usbjack_imp_range);

/*
 * Workqueue handle
 */
static void s5m3700x_mdet_chk_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, mdet_chk_work.work);
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct device *dev = s5m3700x->dev;
	unsigned int mdet_status = 0;

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	s5m3700x_read_only_hardware(s5m3700x, S5M3700X_0F0_STATUS1, &mdet_status);

	dev_info(dev, "%s called. MDET R: %02X\n", __func__, mdet_status);
	if (mdet_status & ANT_MDET_DET_MASK) {
		cancel_delayed_work(&jackdet->mdet_chk_work);
		s5m3700x_usbjackstate_set(s5m3700x, USB_MCHK_DONE);
		jack_wake_unlock(jackdet->jack_wakeup);
	} else {
		/* lock for jack and button irq */
		jack_wake_unlock(jackdet->jack_wakeup);

		/* Read mdet status ongoing */
		cancel_delayed_work(&jackdet->mdet_chk_work);
		queue_delayed_work(jackdet->mdet_chk_wq, &jackdet->mdet_chk_work,
				msecs_to_jiffies(MDET_CHK_DELAY));
	}
}

static void s5m3700x_usbjack_det_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, jack_det_work.work);
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct device *dev = s5m3700x->dev;
	int jack_state = USB_4POLE;
	int mdet_adc = 0;

	mutex_lock(&jackdet->jack_det_lock);
	dev_info(dev, "%s called, cur_jack_state: %s.\n",
			__func__, s5m3700x_return_status_name(jackdet->cur_jack_state));

	/* Pole value decision */
	if (jackdet->cur_jack_state & USB_POLE_DEC) {
		/* Read mdet adc value for detect mic */
		mdet_adc = read_usb_adc(s5m3700x, ADC_POLE, JACK_MDET_ADC, 1);

#if IS_ENABLED(CONFIG_USB_SW_MAX20328B)
		if (jackdet->switch_ic_state == USB_DIRECT_CONNECTION)
			max20328_switch_mode_event(MAX_USBC_SWITCH_SBU_DIRECT_CONNECT);
		else
			max20328_switch_mode_event(MAX_USBC_SWITCH_SBU_FLIP_CONNECT);
#endif

		if (mdet_adc < jackdet->mic_adc_range[0])
			jack_state = USB_3POLE;
		else
			jack_state = USB_4POLE;

		/* Report jack type */
		s5m3700x_usbjackstate_set(s5m3700x, jack_state);
		s5m3700x_report_jack_type(s5m3700x, S5M3700X_USBJACK_MASK, 1);

		dev_info(dev, "%s mic adc: %d, usb type: %s\n", __func__,
				mdet_adc, s5m3700x_print_jack_type_state(jackdet->cur_jack_state));
	} else {
		dev_info(dev, "%s called, current Jack is USB_OUT\n", __func__);
		s5m3700x_report_jack_type(s5m3700x, S5M3700X_USBJACK_MASK, 0);
	}

	dev_info(dev, "%s called, USB %s, Mic %s\n", __func__,
			(jackdet->cur_jack_state & USB_IN) ?	"inserted" : "removed",
			(jackdet->cur_jack_state & USB_4POLE) ? "inserted" : "removed");

	mutex_unlock(&jackdet->jack_det_lock);
	jack_wake_unlock(jackdet->jack_wakeup);
}

static void s5m3700x_usb_button_release_work(struct work_struct *work)
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

static void s5m3700x_usb_button_press_work(struct work_struct *work)
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

	btn_adc = read_usb_adc(s5m3700x, ADC_BTN, JACK_MDET_ADC, 0);

	s5m3700x_report_button_type(s5m3700x, S5M3700X_BUTTON_MASK, BUTTON_PRESS, btn_adc);

	jack_wake_unlock(jackdet->jack_wakeup);
}

/*
 * Jack Detection Process Routine Start
 * handler -> jackstate_set -> jackstate_register -> workqueue (if neeeded)
 */

//jackstate_register
static void s5m3700x_usbout_register(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC4))
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);

	jackdet->adc_re_read = 0;
	jackdet->switch_ic_state = USB_DIRECT_CONNECTION;

	/* Disabled ANT */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C0_ACTR_JD1, PDB_ANT_MDET_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0D2_DCTR_TEST2, T_PDB_ANT_MDET_MASK, 0);

	s5m3700x_write(s5m3700x, S5M3700X_0D6_DCTR_TEST6, 0x01);

	/* Disabled TEST Mode */
	s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0x0C);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FE_DCTR_GP1, T_CNT_ADC_DIV_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0FF_DCTR_GP2, T_ADC_STARTTIME_MASK, 0);

	/* Disabled TEST Mode in IMP CAL */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0EC_DCTR_IMP6, T_IMP_SEQ_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 0 << IMP_VAL_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, SEL_MDET_JO_SIG_MASK, SEL_MDET_JO_SIG_MASK);

	/* Reset AP and Pole in machine */
	s5m3700x_write(s5m3700x, S5M3700X_0E0_DCTR_FSM1, 0x06);
	s5m3700x_write(s5m3700x, S5M3700X_0E3_DCTR_FSM4, 0x40);

	/* HP PULLDN */
	s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_HP_PDN_MASK, EN_HP_PDN_MASK);

	s5m3700x_usb_micbias(false);

	/* LDET OUT */
	s5m3700x_write(s5m3700x, S5M3700X_0D5_DCTR_TEST5, 0xC3);
}

static void s5m3700x_usbinit_register(struct s5m3700x_priv *s5m3700x)
{
	/* Enable MCB2 */
	s5m3700x_usb_micbias(true);

#if IS_ENABLED(CONFIG_USB_SW_MAX20328B)
	max20328_switch_mode_event(MAX_USBC_SWITCH_ENABLE);
#endif
	s5m3700x_write(s5m3700x, S5M3700X_0D5_DCTR_TEST5, 0x82);
}

static void s5m3700x_usb_cmp_register(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* Send the jack out event to the audio framework */
	s5m3700x_report_jack_type(s5m3700x, S5M3700X_USBJACK_MASK, 0);

	/* T_JACK_STATE */
	s5m3700x_write(s5m3700x, S5M3700X_0E3_DCTR_FSM4, 0x40);

	/*
	 * Enable MCB2
	 * It is need code. Cause Using mdet workqueue function.
	 */
	s5m3700x_usb_micbias(true);

	/* 5-Pin Setting */
	s5m3700x_write(s5m3700x, S5M3700X_0E0_DCTR_FSM1, 0x06);

	s5m3700x_update_bits(s5m3700x, S5M3700X_0EC_DCTR_IMP6, T_IMP_SEQ_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, IMP_VAL_MASK, 0 << IMP_VAL_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, SEL_MDET_JO_SIG_MASK, SEL_MDET_JO_SIG_MASK);
	s5m3700x_write(s5m3700x, S5M3700X_0FD_ACTR_GP, 0x0C);
	s5m3700x_write(s5m3700x, S5M3700X_0D6_DCTR_TEST6, 0x01);

	/* ANT Enable */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0D2_DCTR_TEST2, T_PDB_ANT_MDET_MASK, T_PDB_ANT_MDET_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C0_ACTR_JD1, PDB_ANT_MDET_MASK, PDB_ANT_MDET_MASK);

	/* Read mdet status start */
	cancel_delayed_work(&jackdet->mdet_chk_work);
	queue_delayed_work(jackdet->mdet_chk_wq, &jackdet->mdet_chk_work,
			msecs_to_jiffies(MDET_CHK_DELAY));
}

static void s5m3700x_usb_mchk_done_register(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	unsigned int value1, value2;

	/* Reading Mic adc in manual */
	value1 = read_manual_adc_switch(s5m3700x);

	/*
	 * Switching MIC-GND
	 * LRMG => LRGM
	 */
#if IS_ENABLED(CONFIG_USB_SW_MAX20328B)
	max20328_switch_mode_event(MAX_MIC_GND_SWAP);
#endif
	s5m3700x_usleep(5000);
	value2 = read_manual_adc_switch(s5m3700x);

	dev_info(dev, "LRMG: %d, LRGM: %d\n", value1, value2);

	/*
	 * Switching GND-MIC
	 * LRGM => LRMG
	 */
	if (value1 > value2) {
#if IS_ENABLED(CONFIG_USB_SW_MAX20328B)
		max20328_switch_mode_event(MAX_GND_MIC_SWAP);
#endif
		s5m3700x_usleep(5000);
		jackdet->switch_ic_state = USB_DIRECT_CONNECTION;
	} else
		jackdet->switch_ic_state = USB_FLIP_CONNECTION;

	/* T_JACK_STATE */
	s5m3700x_write(s5m3700x, S5M3700X_0E3_DCTR_FSM4, 0x00);
}

static void s5m3700x_usb_pole_dec_register(struct s5m3700x_priv *s5m3700x)
{
	return;
}

static void s5m3700x_usb_3pole_register(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_write(s5m3700x, S5M3700X_0D6_DCTR_TEST6, 0x81);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0EE_ACTR_ETC1, SEL_MDET_JO_SIG_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E3_DCTR_FSM4,
			POLE_VALUE_MASK, POLE_VALUE_3POLE << POLE_VALUE_SHIFT);
}

static void s5m3700x_usb_4pole_register(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_write(s5m3700x, S5M3700X_0D6_DCTR_TEST6, 0x21);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0E3_DCTR_FSM4,
			POLE_VALUE_MASK, POLE_VALUE_4POLE << POLE_VALUE_SHIFT);
}

static bool s5m3700x_usbjackstate_register(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	unsigned int cur_jack, prv_jack;

	cur_jack = jackdet->cur_jack_state;
	prv_jack = jackdet->prv_jack_state;

	if (!s5m3700x_check_jack_state_sequence(s5m3700x))
		return false;

	s5m3700x_print_status_change(dev, prv_jack, cur_jack);
	switch (jackdet->cur_jack_state) {
	case USB_OUT:
		s5m3700x_usbout_register(s5m3700x);
		break;
	case USB_INIT:
		s5m3700x_usbinit_register(s5m3700x);
		break;
	case USB_JACK_CMP:
		s5m3700x_usb_cmp_register(s5m3700x);
		break;
	case USB_MCHK_DONE:
		s5m3700x_usb_mchk_done_register(s5m3700x);
		break;
	case USB_POLE_DEC:
		s5m3700x_usb_pole_dec_register(s5m3700x);
		break;
	case USB_3POLE:
		s5m3700x_usb_3pole_register(s5m3700x);
		break;
	case USB_4POLE:
		s5m3700x_usb_4pole_register(s5m3700x);
		break;
	}

	return true;
}

//jackstate_set
static void s5m3700x_usbjackstate_set(struct s5m3700x_priv *s5m3700x,
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
		ret = s5m3700x_usbjackstate_register(s5m3700x);
		dev_info(dev, "Jack register write %s.\n", ret ? "complete" : "incomplete");
	} else {
		dev_info(dev, "Prv_jack_state and Cur_jack_state are same (%s).\n",
					s5m3700x_return_status_name(jackdet->prv_jack_state));
	}
}

// jack out handler
static void s5m3700x_st_usbjackout_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

#if IS_ENABLED(CONFIG_USB_SW_MAX20328B)
	max20328_switch_mode_event(MAX_USBC_SWITCH_DISABLE);
#endif
	s5m3700x_usbjackstate_set(s5m3700x, USB_OUT);

	cancel_delayed_work(&jackdet->mdet_chk_work);
	cancel_delayed_work(&jackdet->jack_det_work);
	queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
			msecs_to_jiffies(0));
}

// USB Gender detection
static void s5m3700x_st_usbjack_in_handler(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_usbjackstate_set(s5m3700x, USB_INIT);
}

// Completely Jack In
static void s5m3700x_st_cmp_usbjack_in_handler(struct s5m3700x_priv *s5m3700x)
{
	s5m3700x_usbjackstate_set(s5m3700x, USB_JACK_CMP);
}

// Read MIC Impedance for pole decision
// Read Left Impedance for OMTP decision
// report jack type
static void s5m3700x_st_usbjackdet_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	if (jackdet->adc_re_read == 1) {
		jackdet->adc_re_read = 0;
		return;
	}
	s5m3700x_usbjackstate_set(s5m3700x, USB_POLE_DEC);

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	cancel_delayed_work(&jackdet->jack_det_work);
	queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
			msecs_to_jiffies(JACK_MDET_DELAY));
}

static void s5m3700x_usb_btn_release_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	/* Ear-mic BST4 Reset */
	reset_mic4_boost(s5m3700x);

	queue_delayed_work(jackdet->btn_release_wq, &jackdet->btn_release_work,
			msecs_to_jiffies(BTN_ADC_DELAY));
}

static void s5m3700x_usb_btn_press_handler(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	queue_delayed_work(jackdet->btn_press_wq, &jackdet->btn_press_work,
			msecs_to_jiffies(BTN_ADC_DELAY));
}

static int s5m3700x_usbjack_handler(struct s5m3700x_priv *s5m3700x, unsigned int event)
{
	struct device *dev = s5m3700x->dev;
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	int irq_type = s5m3700x_get_irq_type(s5m3700x, event);

	mutex_lock(&jackdet->key_lock);

	switch (irq_type) {
	case IRQ_ST_JACKOUT:
		dev_info(dev, "[USB IRQ] %s USB Jack out interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_usbjackout_handler(s5m3700x);
		break;
	case IRQ_ST_JACKIN:
		dev_info(dev, "[USB IRQ] %s CCIC det interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_usbjack_in_handler(s5m3700x);
		break;
	case IRQ_ST_CMP_JACK_IN:
		dev_info(dev, "[IRQ] %s Completely Jack in interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_cmp_usbjack_in_handler(s5m3700x);
		break;
	case IRQ_ST_JACKDET:
		dev_info(dev, "[IRQ] %s Jack det interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_st_usbjackdet_handler(s5m3700x);
		break;
	case IRQ_ST_BTN_RELEASE:
		dev_info(dev, "[IRQ] %s Button Release interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_usb_btn_release_handler(s5m3700x);
		break;
	case IRQ_ST_BTN_PRESS:
		dev_info(dev, "[IRQ] %s Button Press interrupt, line: %d\n", __func__, __LINE__);
		s5m3700x_usb_btn_press_handler(s5m3700x);
	default:
		dev_info(dev, "[USB IRQ] %s IRQ return type skip, line %d\n", __func__, __LINE__);
		break;
	}

	mutex_unlock(&jackdet->key_lock);

	return IRQ_HANDLED;
}

/*
 * Jack Detection Process Routine End
 */

static void s5m3700x_usbjack_parse_dt(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	int ret = 0;
	int usb_hp_chl, usb_hp_chr;

	s5m3700x_common_parse_dt(s5m3700x);

	/* USB HP Offset Cal */
	ret = of_property_read_u32(dev->of_node, "usb-hp-chl", &usb_hp_chl);
	if (!ret)
		jackdet->usb_hp_chl = usb_hp_chl;
	else
		jackdet->usb_hp_chl = S5M3700X_USB_HP_CHL_OFFSET;

	ret = of_property_read_u32(dev->of_node, "usb-hp-chr", &usb_hp_chr);
	if (!ret)
		jackdet->usb_hp_chr = usb_hp_chr;
	else
		jackdet->usb_hp_chr = S5M3700X_USB_HP_CHR_OFFSET;

	dev_info(dev, "USB HP Offset Ch-L: %d, Ch-R: %d\n",
			jackdet->usb_hp_chl, jackdet->usb_hp_chr);
}

/* Register workqueue */
static int s5m3700x_register_usbjack_workqueue(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* Initialize workqueue for usb detect handling */
	INIT_DELAYED_WORK(&jackdet->jack_det_work, s5m3700x_usbjack_det_work);
	jackdet->jack_det_wq = create_singlethread_workqueue("usbjack_det_wq");
	if (jackdet->jack_det_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create jack_det_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for mdet detect handling */
	INIT_DELAYED_WORK(&jackdet->mdet_chk_work, s5m3700x_mdet_chk_work);
	jackdet->mdet_chk_wq = create_singlethread_workqueue("mdet_chk_wq");
	if (jackdet->mdet_chk_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create mdet_chk_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for button press, release */
	INIT_DELAYED_WORK(&jackdet->btn_release_work, s5m3700x_usb_button_release_work);
	jackdet->btn_release_wq = create_singlethread_workqueue("btn_release_wq");
	if (jackdet->btn_release_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create btn_release_wq\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&jackdet->btn_press_work, s5m3700x_usb_button_press_work);
	jackdet->btn_press_wq = create_singlethread_workqueue("btn_press_wq");
	if (jackdet->btn_press_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create btn_press_wq\n");
		return -ENOMEM;
	}
	return 0;
}

/* EXPORT SYMBOL to notify jack insert or remove status from CCIC to usb jack driver */
void s5m3700x_usbjack_call_notifier(unsigned int event)
{
	struct s5m3700x_jack *jackdet = NULL;
	struct s5m3700x_priv *s5m3700x = NULL;

	if (p_s5m3700x_jack != NULL) {
		jackdet = p_s5m3700x_jack;
		s5m3700x = jackdet->p_s5m3700x;

		dev_info(s5m3700x->dev, "%s called, event: %d\n", __func__, event);
		s5m3700x_call_notifier(s5m3700x, event);
	}
}
EXPORT_SYMBOL_GPL(s5m3700x_usbjack_call_notifier);

/* IRQ Process Thread Routine */
irqreturn_t s5m3700x_usbjack_irq_thread(int irq, void *irq_data)
{
	struct s5m3700x_jack *jackdet = irq_data;
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;

	dev_info(s5m3700x->dev, "%s called.\n", __func__);

	/* call notifier handler */
	s5m3700x_call_notifier(s5m3700x, JACK_HANDLER_EVENT_NONE);

	return IRQ_HANDLED;
}

/*
 * s5m3700x_usbjack_notifier_handler() - Codec IRQ Handler
 *
 * Desc: Set codec register according to codec IRQ.
 */
static int s5m3700x_usbjack_notifier_handler(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct s5m3700x_priv *s5m3700x = data;

	dev_info(s5m3700x->dev, "%s called. event: %d\n", __func__, event);

	/* read irq registgers */
	s5m3700x_read_irq(s5m3700x);

	s5m3700x_usbjack_handler(s5m3700x, event);

	return IRQ_HANDLED;
}


/* apply register initialize by patch
 * patch is only updated hw register, so cache need to be updated.
 */
static int s5m3700x_regmap_usbjack_register_patch(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;
	int ret = 0;

	/* usbjack register patch */
	ret = regmap_register_patch(s5m3700x->regmap, s5m3700x_usbjack_patch,
				ARRAY_SIZE(s5m3700x_usbjack_patch));
	if (ret < 0) {
		dev_err(dev, "Failed to apply s5m3700x_usbjack_patch %d\n", ret);
		return ret;
	}

	/* update reg_defaults with registered patch */
	s5m3700x_update_reg_defaults(s5m3700x_usbjack_patch, ARRAY_SIZE(s5m3700x_usbjack_patch));
	return ret;
}

static void s5m3700x_usbjack_register_initialize(struct s5m3700x_priv *s5m3700x)
{
	/* update initial registers on cache and hw registers */
	s5m3700x_regmap_usbjack_register_patch(s5m3700x);

	/* reinit regmap cache because reg_defaults are updated*/
	s5m3700x_regmap_reinit_cache(s5m3700x);
}

static void s5m3700_usb_hp_trim(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	unsigned int t_cond[8], trim_val;
	u32 cond[2];
	unsigned int i = 0;
	int sft_table[25] = {90, 81, 72, 64, 57, 51, 46, 41, 36, 33, 29,
		26, 47, 42, 37, 34, 30, 27, 24, 22, 20, 18, 16, 14, 13};
	unsigned int cal_table[25], tune_val = 0;
	bool cond_flag;

	dev_info(s5m3700x->dev, "%s called, setting usb hp trim values.\n", __func__);

	s5m3700x_read(s5m3700x, S5M3700X_25F_HPLOFF_S_3, &t_cond[0]);
	s5m3700x_read(s5m3700x, S5M3700X_25E_HPLOFF_S_2, &t_cond[1]);
	s5m3700x_read(s5m3700x, S5M3700X_25D_HPLOFF_S_1, &t_cond[2]);
	s5m3700x_read(s5m3700x, S5M3700X_25C_HPLOFF_S_0, &t_cond[3]);
	s5m3700x_read(s5m3700x, S5M3700X_263_HPROFF_S_3, &t_cond[4]);
	s5m3700x_read(s5m3700x, S5M3700X_262_HPROFF_S_2, &t_cond[5]);
	s5m3700x_read(s5m3700x, S5M3700X_261_HPROFF_S_1, &t_cond[6]);
	s5m3700x_read(s5m3700x, S5M3700X_260_HPROFF_S_0, &t_cond[7]);

	cond[0] = t_cond[0] + (t_cond[1] << 8) + (t_cond[2] << 16) + (t_cond[3] << 24);
	cond[1] = t_cond[4] + (t_cond[5] << 8) + (t_cond[6] << 16) + (t_cond[7] << 24);

	dev_info(s5m3700x->dev, "%s called, 0: 0x%08x 1: 0x%08x\n",
			__func__, cond[0], cond[1]);

	/* L Ch */
	for (i = 0; i < 25; i++)
		cal_table[i] = jackdet->usb_hp_chl / sft_table[i];

	for (i = 0; i < 25; i++) {
		s5m3700x_read(s5m3700x, S5M3700X_200_HP_OFFSET_L0 + i, &trim_val);
		cond_flag = cond[0] & BIT(i);
		if (cond_flag) {
			tune_val = trim_val + cal_table[i];
			if (tune_val >= 0xff)
				tune_val = 0xff;
		} else {
			if (trim_val > cal_table[i]) {
				tune_val = trim_val - cal_table[i];
			} else {
				tune_val = cal_table[i] - trim_val;
				cond[0] = cond[0] | BIT(i);
			}
		}
		s5m3700x_write(s5m3700x, S5M3700X_200_HP_OFFSET_L0 + i, tune_val);
	}

	/* R Ch */
	for (i = 0; i < 25; i++)
		cal_table[i] = jackdet->usb_hp_chr / sft_table[i];

	for (i = 0; i < 25; i++) {
		s5m3700x_read(s5m3700x, S5M3700X_219_HP_OFFSET_R0 + i, &trim_val);
		cond_flag = cond[1] & BIT(i);
		if (cond_flag) {
			tune_val = trim_val + cal_table[i];
			if (tune_val >= 0xff)
				tune_val = 0xff;
		} else {
			if (trim_val > cal_table[i]) {
				tune_val = trim_val - cal_table[i];
			} else {
				tune_val = cal_table[i] - trim_val;
				cond[1] = cond[1] | BIT(i);
			}
		}
		s5m3700x_write(s5m3700x, S5M3700X_219_HP_OFFSET_R0 + i, tune_val);
	}
	dev_info(s5m3700x->dev, "%s called, 0: 0x%08x 1: 0x%08x\n",
			__func__, cond[0], cond[1]);

	s5m3700x_write(s5m3700x, S5M3700X_25F_HPLOFF_S_3, (cond[0] & MASK(8, 0)));
	s5m3700x_write(s5m3700x, S5M3700X_25E_HPLOFF_S_2, (cond[0] & MASK(8, 8)) >> 8);
	s5m3700x_write(s5m3700x, S5M3700X_25D_HPLOFF_S_1, (cond[0] & MASK(8, 16)) >> 16);
	s5m3700x_write(s5m3700x, S5M3700X_25C_HPLOFF_S_0, (cond[0] & MASK(8, 24)) >> 24);
	s5m3700x_write(s5m3700x, S5M3700X_263_HPROFF_S_3, (cond[1] & MASK(8, 0)));
	s5m3700x_write(s5m3700x, S5M3700X_262_HPROFF_S_2, (cond[1] & MASK(8, 8)) >> 8);
	s5m3700x_write(s5m3700x, S5M3700X_261_HPROFF_S_1, (cond[1] & MASK(8, 16)) >> 16);
	s5m3700x_write(s5m3700x, S5M3700X_260_HPROFF_S_0, (cond[1] & MASK(8, 24)) >> 24);
}

/*
 * s5m3700x_usbjack_probe() - Initialize variable related jack
 *
 * @codec: SoC audio codec device
 *
 * Desc: This function is called by s5m3700x_component_probe. For separate codec
 * and jack code, this function called from codec driver. This function initialize
 * jack variable, workqueue, mutex, and wakelock.
 */

int s5m3700x_usbjack_probe(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet;
	int ret = 0;

	dev_info(s5m3700x->dev, "Codec Jack Probe: (%s)\n", __func__);

	jackdet = kzalloc(sizeof(struct s5m3700x_jack), GFP_KERNEL);
	if (jackdet == NULL)
		return -ENOMEM;

	/* initialize variable */
	s5m3700x_jack_variable_initialize(s5m3700x, jackdet);

	/* allocate global variable to access from CCIC driver*/
	p_s5m3700x_jack = jackdet;

	/* initialize variable for local value */
	jackdet->prv_jack_state = USB_OUT;
	jackdet->cur_jack_state = USB_OUT;
	jackdet->switch_ic_state = USB_DIRECT_CONNECTION;

	/* Device Tree for jack */
	s5m3700x_usbjack_parse_dt(s5m3700x);

	/* Register workqueue */
	s5m3700x_register_usbjack_workqueue(s5m3700x);

	/* Register jack */
	s5m3700x_register_jack(codec, s5m3700x, S5M3700X_USBJACK_MASK);

	/* Register Button */
	s5m3700x_register_button(codec, s5m3700x, S5M3700X_BUTTON_MASK);

	/* s5m3700x_resume */
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3700x->dev))
		pm_runtime_get_sync(s5m3700x->dev);
#endif

	s5m3700x_usbjack_register_initialize(s5m3700x);

	/* USB HP trim */
	s5m3700_usb_hp_trim(s5m3700x);

	/* restore hp_trim */
	s5m3700x_hp_trim_initialize(s5m3700x);

	/* Configure mic bias voltage */
	s5m3700x_configure_mic_bias(s5m3700x);

	/* Jack OTP Read */
	s5m3700x_read_imp_otp(s5m3700x);

	/* Register Jack controls */
	s5m3700x_register_jack_controls(codec, s5m3700x);

	/* Register usb jack detection notifier */
	s5m3700x_register_notifier(s5m3700x, s5m3700x_usbjack_notifier_handler);

	/* Request Jack IRQ */
	s5m3700x_request_threaded_irq(s5m3700x, s5m3700x_usbjack_irq_thread);

	/* s5m3700x_suspend */
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3700x->dev))
		pm_runtime_put_sync(s5m3700x->dev);
#endif
	jackdet->is_probe_done = true;
	return ret;
}

int s5m3700x_usbjack_remove(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	dev_info(codec->dev, "(*) %s called\n", __func__);

	destroy_workqueue(jackdet->mdet_chk_wq);
	destroy_workqueue(jackdet->jack_det_wq);
	destroy_workqueue(jackdet->btn_press_wq);
	destroy_workqueue(jackdet->btn_release_wq);

	s5m3700x_jack_register_exit(codec);

	/* Unregister ADC pin */
	kfree(jackdet);

	return 0;
}
