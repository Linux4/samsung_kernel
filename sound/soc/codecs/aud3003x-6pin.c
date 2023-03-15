/*
 * sound/soc/codec/aud3003x-6pin.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <linux/input.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/mfd/samsung/s2mpu10-regulator.h>
#include <sound/aud3003x.h>
#include <uapi/linux/input-event-codes.h>

#include "aud3003x.h"
#include "aud3003x-6pin.h"

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err
#endif

static void adc_start(struct aud3003x_jack *jackdet)
{
	jackdet->jack_adc_iio = iio_channel_get_all(jackdet->p_aud3003x->dev);

	if (IS_ERR(jackdet->jack_adc_iio))
		dev_err(jackdet->p_aud3003x->dev, "%s call, cannot get adc pin %d\n",
				__func__, PTR_ERR(jackdet->jack_adc_iio));
}

static void adc_stop(struct aud3003x_jack *jackdet)
{
	iio_channel_release(jackdet->jack_adc_iio);
}

static int get_avg(int *values, int total)
{
	int sum = 0;
	int i;

	for (i = 0; i < total; i++)
		sum += values[i];

	sum /= total;

	return sum;
}

static int get_devi(int avg, int *values, int total)
{
	int devi = 0, diff = 0;
	int i;

	for (i = 0; i < total; i++) {
		diff = values[i] - avg;
		devi += (diff * diff);
	}

	return devi;
}

/*
 * adc_get_value() - Get adc data from AP GPADC
 *
 * @jackdet: jack information struct
 * @adc_mode: If it checks GDET adc, adc_mode is 0. If it checks MDET adc,
 * adc_mode is 1.
 *
 * Desc: When something is inserted at ear-jack house, the codec checks
 * GDET adc data for separate the water or the ear-jack at first.
 * After, it checks MDET adc data for distinguish between
 * 3 pole and 4 pole ear-jack.
 * Return: Average of adc data
 */

#define ADC_SAMPLE_SIZE 5

static int adc_get_value(struct aud3003x_jack *jackdet, int adc_mode)
{
	struct iio_channel *jack_adc_iio = jackdet->jack_adc_iio;
	int adc_data = -1;
	int adc_total = 0;
	int adc_retry_cnt = 0;
	int i;

	for (i = 0; i < ADC_SAMPLE_SIZE; i++) {
		iio_read_channel_raw(&jack_adc_iio[adc_mode], &adc_data);

		/* If adc_data is negative, ignore */
		while (adc_data < 0) {
			adc_retry_cnt++;
			if (adc_retry_cnt > 10)
				return adc_data;
			iio_read_channel_raw(&jack_adc_iio[adc_mode], &adc_data);
		}
		adc_total += adc_data;
	}
	return adc_total / ADC_SAMPLE_SIZE;
}

/*
 * mute_mic() - Mute mic if it is active
 *
 * @aud3003x: codec information struct
 * @on: mic mute is true, mic unmute is false
 *
 * Desc: It can mute adc depending on the parameter
 */
static void mute_mic(struct aud3003x_priv *aud3003x, bool on)
{
	struct snd_soc_component *codec = aud3003x->codec;

	dev_dbg(aud3003x->dev, "%s called, %s\n", __func__, on ? "Mute" : "Unmute");

	if (on)
		aud3003x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
	else
		aud3003x_adc_digital_mute(codec, ADC_MUTE_ALL, false);
}

/*
 * button_adc_cal() - Calculate button adc value
 *
 * @jackdet: jack information struct
 *
 * Desc: It calculate mdet adc for check button state.
 * To improve the accuracy of measured adc values, there is a specific delay.
 * ADC_TRACE_SET sets are active, there is ADC_SET_DELAY_US delay between each set.
 * And adc is checked ADC_TRACE_NUM times in one set,
 * there is ADC_READ_DELAY_US delay between each times.
 * If an error adc is checked after read adc, retry after ADC_READY_DELAY_MS.
 * Return: adc value is true, error code is false.
 */

#define ADC_TRACE_NUM		5
#define ADC_TRACE_SET		2
#define ADC_READY_DELAY_MS	10
#define ADC_READ_DELAY_US	500
#define ADC_SET_DELAY_US	1000
#define ADC_DEVI_THRESHOLD	20000

static int button_adc_cal(struct aud3003x_jack *jackdet)
{
	struct device *dev = jackdet->p_aud3003x->dev;
	int adc_values[ADC_TRACE_NUM];
	int set, num, avg, devi;
	int adc_max = 0;

	/* Try ADC_TRACE_SET times */
	for (set = 0; set < ADC_TRACE_SET; set++) {
		/* Read mdet adc for button ADC_TRACE_NUM times */
		for (num = 0; num < ADC_TRACE_NUM; num++) {
			adc_values[num] = adc_get_value(jackdet, 1);
			aud3003x_usleep(ADC_READ_DELAY_US);
		}

		/* Check avg/devi value */
		avg = get_avg(adc_values, ADC_TRACE_NUM);
		devi = get_devi(avg, adc_values, ADC_TRACE_NUM);
		dev_dbg(dev, "%s: button adc avg: %d, devi: %d\n", __func__, avg, devi);

		/* Retry if devi exceeds the threshold */
		if (devi > ADC_DEVI_THRESHOLD) {
			queue_delayed_work(jackdet->buttons_wq, &jackdet->buttons_work,
					msecs_to_jiffies(ADC_READY_DELAY_MS));
			dev_err(dev, "%s: retry button_work: %d %d %d %d %d\n",
					__func__,
					adc_values[0],
					adc_values[1],
					adc_values[2],
					adc_values[3],
					adc_values[4]);
			return -EINVAL;
		}

		if (avg > adc_max)
			adc_max = avg;

		aud3003x_usleep(ADC_SET_DELAY_US);
	}

	return adc_max;
}

/*
 * process_button_ev() - Process the button events
 *
 * @jackdet: jack information struct
 * @on: button press is true, button release is false.
 *
 * Desc: It processes button press or release events. It gives information
 * about the button state to framework layer.
 */
static void process_button_ev(struct aud3003x_jack *jackdet, bool on)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct aud3003x_priv *aud3003x = jackdet->p_aud3003x;

	dev_dbg(aud3003x->dev, "%s: key %d is %s, adc: %d\n",
			__func__, jackstate->button_code,
			on ? "pressed" : "released", jackstate->btn_adc);

	input_report_key(jackdet->input, jackstate->button_code, on);
	input_sync(jackdet->input);
	mute_mic(aud3003x, on);
}

/*
 * before_button_error_chk() - Check button state error
 *
 * @jackdet: jack information struct
 *
 * Desc: Error check condition for button irq before current button state check.
 * Return: There is not problem, return true.
 */
static bool before_button_error_chk(struct aud3003x_jack *jackdet)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_aud3003x->dev;

	/* Terminate workqueue Cond1: jack is not detected */
	if (jackstate->cur_jack_state & JACK_OUT) {
		dev_err(dev, "Skip button events because jack is not detected.\n");
		if (jackstate->prv_btn_state == BUTTON_PRESS) {
			jackstate->prv_btn_state = BUTTON_RELEASE;
			process_button_ev(jackdet, BUTTON_RELEASE);
		}
		return false;
	}

	/* Terminate workqueue Cond2: 3 pole earjack */
	if (!(jackstate->cur_jack_state & JACK_4POLE)) {
		dev_err(dev, "Skip button events for 3 pole earjack.\n");
		return false;
	}

	/* Terminate workqueue Cond3: button adc error */
	if (jackstate->btn_adc == -EINVAL) {
		dev_err(dev, "Button ADC Error! BTN ADC: %d\n", jackstate->btn_adc);
		return false;
	}

	return true;
}

/*
 * after_button_error_chk() - Check button state error
 *
 * @jackdet: jack information struct
 *
 * Desc: Error check condition for button irq after current button state check.
 * Return: There is not problem, return true.
 */
static bool after_button_error_chk(struct aud3003x_jack *jackdet)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_aud3003x->dev;

	/* Terminate workqueue Cond4: 3 pole earjack with button press */
	if ((jackstate->cur_btn_state == BUTTON_PRESS) &&
			!(jackstate->cur_jack_state & JACK_4POLE)) {
		dev_err(dev, "Skip button press event for 3 pole earjack.\n");
		jackstate->prv_btn_state = BUTTON_RELEASE;
		process_button_ev(jackdet, BUTTON_RELEASE);
		return false;
	}

	/* Terminate workqueue Cond5: button state not changed with button press */
	if ((jackstate->cur_btn_state == BUTTON_PRESS) &&
			(jackstate->prv_btn_state == jackstate->cur_btn_state)) {
		dev_err(dev, "Skip button press event, button status are same.\n");
		jackstate->prv_btn_state = BUTTON_RELEASE;
		process_button_ev(jackdet, BUTTON_RELEASE);
		return false;
	}

	/* Terminate workqueue Cond6: button state not changed */
	if (jackstate->prv_btn_state == jackstate->cur_btn_state) {
		dev_err(dev, "Button status are same.\n");
		return false;
	}

	return true;
}

void reset_mic4_boost(struct aud3003x_jack *jackdet)
{
	struct aud3003x_priv *aud3003x = jackdet->p_aud3003x;
	struct snd_soc_component *codec = jackdet->codec;

	regcache_cache_switch(aud3003x, false);
	i2c_client_change(aud3003x, AUD3003D);

	/* RESETB testmode on */
	snd_soc_component_update_bits(codec, AUD3003X_B5_ODSEL6,
			T_RESETB_BST4_MASK, T_RESETB_BST4_MASK);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	i2c_client_change(aud3003x, AUD3003A);

	/* Boost Reset */
	snd_soc_component_update_bits(codec, AUD3003X_11C_BSTC, RESETB_BST4_MASK, 0);

	i2c_client_change(aud3003x, CODEC_CLOSE);

	/* DSM Testmode On */
	i2c_client_change(aud3003x, AUD3003D);
	snd_soc_component_update_bits(codec, AUD3003X_B5_ODSEL6,
			T_RESETB_INTR_MASK | T_RESETB_QUANTR_MASK,
			T_RESETB_INTR_MASK | T_RESETB_QUANTR_MASK);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	aud3003x_usleep(100);

	/* DSM Reset */
	i2c_client_change(aud3003x, AUD3003D);
	snd_soc_component_update_bits(codec, AUD3003X_B5_ODSEL6,
			T_RESETB_INTR_MASK | T_RESETB_QUANTR_MASK, 0);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	msleep(40);

	i2c_client_change(aud3003x, AUD3003A);

	snd_soc_component_update_bits(codec, AUD3003X_11C_BSTC, RESETB_BST4_MASK, RESETB_BST4_MASK);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	i2c_client_change(aud3003x, AUD3003D);

	/* RESETB testmode off */
	snd_soc_component_update_bits(codec, AUD3003X_B5_ODSEL6, T_RESETB_BST4_MASK, 0);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	regcache_cache_switch(aud3003x, true);
}

/*
 * aud3003x_buttons_work() - Process button interrupt
 *
 * @work: delayed workqueue
 *
 * Desc: When button interrupt is occured, generated buttons_work process.
 * Judge whether button press or release using button adc. And notify to
 * framework layer.
 */
static void aud3003x_buttons_work(struct work_struct *work)
{
	struct aud3003x_jack *jackdet =
		container_of(work, struct aud3003x_jack, buttons_work.work);
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_aud3003x->dev;
	struct jack_buttons_zone *btn_zones = jackdet->jack_buttons_zones;
	int num_buttons_zones = ARRAY_SIZE(jackdet->jack_buttons_zones);
	int i;

	dev_info(dev, "%s called.\n", __func__);

	/* Read adc value */
	jackstate->btn_adc = button_adc_cal(jackdet);

	/* Check error status */
	if (!before_button_error_chk(jackdet)) {
		wake_unlock(&jackdet->jack_wake_lock);
		return;
	}

	/* Check button press/release */
	dev_dbg(dev, "Button %s! adc: %d, btn_release_value: %d\n",
			jackstate->btn_adc > jackdet->btn_release_value ? "Released" : "Pressed",
			jackstate->btn_adc,	jackdet->btn_release_value);
	jackstate->cur_btn_state =
		jackstate->btn_adc > jackdet->btn_release_value ? BUTTON_RELEASE : BUTTON_PRESS;

	/* Check error status */
	if (!after_button_error_chk(jackdet)) {
		wake_unlock(&jackdet->jack_wake_lock);
		return;
	}

	/* Save button information */
	jackstate->prv_btn_state = jackstate->cur_btn_state;

	if (jackstate->cur_btn_state == BUTTON_PRESS) {
		/* Determine which button pressed */
		for (i = 0; i < num_buttons_zones; i++) {
			if (jackstate->btn_adc <= btn_zones[i].adc_high) {
				jackstate->button_code = btn_zones[i].code;
				dev_info(dev,
						"%s: adc: %d, btn code: %d, low: %d, high: %d\n",
						__func__, jackstate->btn_adc, jackstate->button_code,
						btn_zones[i].adc_low, btn_zones[i].adc_high);
				process_button_ev(jackdet, BUTTON_PRESS);
				break;
			}
		}
	} else {
		/* Ear-mic BST4 Reset */
		reset_mic4_boost(jackdet);

		/* Button released */
		process_button_ev(jackdet, BUTTON_RELEASE);
	}
	wake_unlock(&jackdet->jack_wake_lock);
}

/*
 * fake_jack_check() - Check fake inserted jack
 *
 * @jackdet: jack information struct
 *
 * Desc: When earjack detected, check whether or not fake inserted jack.
 * If jack was fake inserted, return false. If not, return true.
 */
static bool fake_jack_check(struct aud3003x_jack *jackdet)
{
	struct aud3003x_priv *aud3003x = jackdet->p_aud3003x;
	struct snd_soc_component *codec = jackdet->codec;
	unsigned int jack_status_bit = 0;

	regcache_cache_switch(aud3003x, false);

	/* Check jack det status */
	i2c_client_change(aud3003x, AUD3003D);
	jack_status_bit = snd_soc_component_read32(codec, AUD3003X_F0_STATUS1);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	regcache_cache_switch(aud3003x, true);

	jack_status_bit = jack_status_bit & JACK_DET_MASK;

	if (!jack_status_bit) {
		dev_err(aud3003x->dev, "%s, jack is not inserted.\n", __func__);
		return false;
	}

	return true;
}

/*
 * aud3003x_jackstate_register() - Register setting following jack status
 *
 * @jackdet: jack information struct
 *
 * Desc: Whenever the jack state was changed, write the required register value.
 * Return: If there is a case for state change, return true. If not, return false.
 */
static bool aud3003x_jackstate_register(struct aud3003x_jack *jackdet)
{
	struct aud3003x_priv *aud3003x = jackdet->p_aud3003x;
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct snd_soc_component *codec = jackdet->codec;
	struct device *dev = jackdet->p_aud3003x->dev;
	unsigned int cur_jack, prv_jack, mic4_on;

	cur_jack = jackstate->cur_jack_state;
	prv_jack = jackstate->prv_jack_state;

	regcache_cache_switch(aud3003x, false);

	switch (cur_jack) {
	case JACK_OUT:
		i2c_client_change(aud3003x, AUD3003D);
		mic4_on = snd_soc_component_read32(codec, AUD3003X_1E_CHOP1) & MIC4_ON_MASK;
		i2c_client_change(aud3003x, CODEC_CLOSE);

		/* For remove tx noise in device switching section */
		if (mic4_on)
			mute_mic(aud3003x, true);

#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
		if (prv_jack & (JACK_IN | JACK_POLE_DEC)) {
#else
		if (prv_jack & (JACK_IN | JACK_POLE_DEC | JACK_AUX)) {
#endif
			if (prv_jack & JACK_3POLE)
				dev_dbg(dev, "Priv: JACK_3POLE -> Cur: JACK_OUT\n");
			if (prv_jack & JACK_4POLE)
				dev_dbg(dev, "Priv: JACK_4POLE -> Cur: JACK_OUT\n");
			if (prv_jack & JACK_POLE_DEC)
				dev_dbg(dev, "Priv: JACK_POLE_DEC -> Cur: JACK_OUT\n");
#ifndef CONFIG_SND_SOC_AUD3003X_EXT_ANT
			if (prv_jack & JACK_AUX)
				dev_dbg(dev, "Priv: JACK_AUX -> Cur: JACK_OUT\n");
#endif
			i2c_client_change(aud3003x, AUD3003D);

			snd_soc_component_write(codec, AUD3003X_E4_DCTR_FSM3, 0x00);
			/* Pole value reset */
			snd_soc_component_write(codec, AUD3003X_E3_DCTR_FSM2, 0x00);
			/* Micbias HQ mode reset */
			snd_soc_component_write(codec, AUD3003X_E5_DCTR_FSM4, 0x02);

			i2c_client_change(aud3003x, CODEC_CLOSE);
			i2c_client_change(aud3003x, AUD3003A);

			/* Auto Power Off OVP */
			snd_soc_component_update_bits(codec, AUD3003X_143_CTRL_OVP1,
					OVP_APON_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else if (prv_jack & JACK_APCHK) {
			dev_dbg(dev, "Priv: JACK_APCHK -> Cur: JACK_OUT\n");
			i2c_client_change(aud3003x, AUD3003D);

			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_JACK_OUT_MASK, AP_JACK_OUT_MASK);
			aud3003x_usleep(100);
			snd_soc_component_write(codec, AUD3003X_E4_DCTR_FSM3, 0x00);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else {
			goto err;
		}
		break;
	case JACK_APCHK:
		if (prv_jack & JACK_OUT) {
			dev_dbg(dev, "Priv: JACK_OUT -> Cur: JACK_APCHK\n");
		} else {
			goto err;
		}
		break;
	case JACK_WATER:
		if (prv_jack & JACK_APCHK) {
			dev_dbg(dev, "Priv: JACK_APCHK -> Cur: JACK_WATER\n");
			i2c_client_change(aud3003x, AUD3003D);

			/* IRQ masking on */
#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
			snd_soc_component_write(codec, AUD3003X_08_IRQ1M, 0x01);
			snd_soc_component_write(codec, AUD3003X_09_IRQ2M, 0x01);
#else
			snd_soc_component_write(codec, AUD3003X_08_IRQ1M, 0x03);
			snd_soc_component_write(codec, AUD3003X_09_IRQ2M, 0x03);
#endif

			/* AP Polling start */
			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_POLLING_MASK, AP_POLLING_MASK);
			aud3003x_usleep(100);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else {
			goto err;
		}
		break;
	case JACK_WATER_OUT:
		if (prv_jack & JACK_WATER) {
			dev_dbg(dev, "Priv: JACK_WATER -> Cur: JACK_WATER_OUT\n");
			i2c_client_change(aud3003x, AUD3003D);

			/* IRQ masking off */
#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
			snd_soc_component_write(codec, AUD3003X_08_IRQ1M, 0x00);
			snd_soc_component_write(codec, AUD3003X_09_IRQ2M, 0x00);
#else
			snd_soc_component_write(codec, AUD3003X_08_IRQ1M, 0x02);
			snd_soc_component_write(codec, AUD3003X_09_IRQ2M, 0x02);
#endif

			/* AP Polling stop */
			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_JACK_ANT_MASK, 0);
			aud3003x_usleep(100);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else {
			goto err;
		}
		break;
#ifndef CONFIG_SND_SOC_AUD3003X_EXT_ANT
	case JACK_AUX:
		if (prv_jack & (JACK_APCHK | JACK_OUT)) {
			dev_dbg(dev, "Priv: JACK_APCHK -> Cur: JACK_AUX\n");
			i2c_client_change(aud3003x, AUD3003D);

			/* ANT MDET Masking */
			snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
					T_PDB_ANT_MDET_MASK, T_PDB_ANT_MDET_MASK);

			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_ANT_RJIN_MASK, AP_ANT_RJIN_MASK);
			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_JACK_IN_MASK, AP_JACK_IN_MASK);
			aud3003x_usleep(100);
			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_JACK_ANT_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else {
			goto err;
		}
		break;
#endif
	case JACK_POLE_DEC:
		if (prv_jack & (JACK_APCHK | JACK_OUT | JACK_3POLE | JACK_4POLE)) {
			if (prv_jack & JACK_APCHK)
				dev_dbg(dev, "Priv: JACK_APCHK -> Cur: JACK_POLE_DEC\n");
			if (prv_jack & JACK_OUT)
				dev_dbg(dev, "Priv: JACK_OUT -> Cur: JACK_POLE_DEC\n");
			if (prv_jack & JACK_3POLE)
				dev_dbg(dev, "Priv: JACK_3POLE -> Cur: JACK_POLE_DEC\n");
			if (prv_jack & JACK_4POLE)
				dev_dbg(dev, "Priv: JACK_4POLE -> Cur: JACK_POLE_DEC\n");

			i2c_client_change(aud3003x, AUD3003D);

			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_JACK_IN_MASK, AP_JACK_IN_MASK);
			aud3003x_usleep(100);
			snd_soc_component_update_bits(codec, AUD3003X_E4_DCTR_FSM3,
					AP_JACK_ANT_MASK, 0);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else {
			goto err;
		}
		break;
	case JACK_3POLE:
#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
		if (prv_jack & JACK_POLE_DEC) {
#else
		if (prv_jack & (JACK_POLE_DEC | JACK_AUX)) {
#endif
			dev_dbg(dev, "Priv: JACK_POLE_DEC -> Cur: JACK_3POLE\n");
			i2c_client_change(aud3003x, AUD3003D);

			/* Pole value : 3pole */
			snd_soc_component_write(codec, AUD3003X_E3_DCTR_FSM2, 0x20);
			aud3003x_usleep(100);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else {
			goto err;
		}
		break;
	case JACK_4POLE:
#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
		if (prv_jack & JACK_POLE_DEC) {
#else
		if (prv_jack & (JACK_POLE_DEC | JACK_AUX)) {
#endif
			dev_dbg(dev, "Priv: JACK_POLE_DEC -> Cur: JACK_4POLE\n");
			i2c_client_change(aud3003x, AUD3003D);

			/* Pole value : 4pole */
			snd_soc_component_write(codec, AUD3003X_E3_DCTR_FSM2, 0x30);
			aud3003x_usleep(100);

			i2c_client_change(aud3003x, CODEC_CLOSE);
		} else {
			goto err;
		}
		break;
	default:
		goto err;
	}

	regcache_cache_switch(aud3003x, true);
	return true;

err:
	regcache_cache_switch(aud3003x, true);
	dev_err(dev, "%s Jack state machine error! prv: 0x%02x, cur: 0x%02x\n",
			__func__, jackstate->prv_jack_state, jackstate->cur_jack_state);
	return false;
}

/*
 * aud3003x_jackstate_set() - Set current jack state
 *
 * @jackdet: jack information struct
 * @change_state: current jack state
 *
 * Desc: After jack inserted, privious jack state was saved and
 * current jack state was updated.
 */
static void aud3003x_jackstate_set(struct aud3003x_jack *jackdet,
		u8 change_state)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_aud3003x->dev;
	bool ret;

	/* Save privious jack state */
	jackstate->prv_jack_state = jackstate->cur_jack_state;

	/* Update current jack state */
	jackstate->cur_jack_state = change_state;

	if (jackstate->prv_jack_state != jackstate->cur_jack_state) {
		dev_dbg(dev, "%s called, Prv: 0x%02x, Cur: 0x%02x\n",  __func__,
				jackstate->prv_jack_state, jackstate->cur_jack_state);

		/* Set jack register */
		ret = aud3003x_jackstate_register(jackdet);
		dev_dbg(dev, "Jack register write %s.\n",
				ret ? "complete" : "incomplete");
	} else {
		dev_dbg(dev, "Prv_jack_state and Cur_jack_state are same.\n");
	}
}

/*
 * aud3003x_jack_det_work() - Process jack interrupt
 *
 * @work: delayed workqueue
 *
 * Desc: When AP check interrupt is occured and adc value is jack,
 * generated jack_det_work process. Judge whether 3 pole or 4 pole jack using mic adc.
 * And notify to framework layer.
 */
static void aud3003x_jack_det_work(struct work_struct *work)
{
	struct aud3003x_jack *jackdet =
		container_of(work, struct aud3003x_jack, jack_det_work.work);
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_aud3003x->dev;

	mutex_lock(&jackdet->mdet_lock);
	dev_dbg(dev, "%s called, cur_jack_state: 0x%02x.\n",
			__func__, jackstate->cur_jack_state);

	/* Pole value decision */
#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
	if (jackstate->cur_jack_state & JACK_POLE_DEC) {
#else
	if (jackstate->cur_jack_state & (JACK_POLE_DEC | JACK_AUX)) {
#endif
		/* Check fake insert condition */
		if (!fake_jack_check(jackdet)) {
			queue_delayed_work(jackdet->gdet_adc_wq, &jackdet->gdet_adc_work,
					msecs_to_jiffies(jackdet->fake_insert_retry_delay));
			mutex_unlock(&jackdet->mdet_lock);
			wake_unlock(&jackdet->jack_wake_lock);
			return;
		}

		/* Read mdet adc value for detect mic */
		jackstate->mdet_adc = adc_get_value(jackdet, 1);

		if (jackstate->mdet_adc > jackdet->mic_adc_range) {
			aud3003x_jackstate_set(jackdet, JACK_4POLE);
			/*
			 * Send the jack detect event to the audio framework
			 * SW_MICROPHONE_INSERT = 0x04 (4 POLE)
			 */
			input_report_switch(jackdet->input, SW_MICROPHONE_INSERT, 1);
		} else {
			aud3003x_jackstate_set(jackdet, JACK_3POLE);
			/*
			 * Send the jack detect event to the audio framework
			 * SW_HEADPHONE_INSERT = 0x02 (3 POLE)
			 */
			input_report_switch(jackdet->input, SW_HEADPHONE_INSERT, 1);
		}
		input_sync(jackdet->input);

		dev_dbg(dev, "%s mic adc: %d, jack type: %s\n", __func__,
				jackstate->mdet_adc,
				(jackstate->cur_jack_state & JACK_4POLE) ? "4POLE" : "3POLE");
#ifndef CONFIG_SND_SOC_AUD3003X_EXT_ANT
	} else if (jackstate->cur_jack_state & JACK_IN) {
		dev_dbg(dev, "%s already inserted jack.\n", __func__);
		input_sync(jackdet->input);
#endif
	} else {
		/* Send the jack out event to the audio framework */
		input_report_switch(jackdet->input, SW_HEADPHONE_INSERT, 0);
		input_report_switch(jackdet->input, SW_MICROPHONE_INSERT, 0);
		input_sync(jackdet->input);
		jackstate->mdet_adc = -EINVAL;
	}

	dev_dbg(dev, "%s called, Jack %s, Mic %s\n", __func__,
			(jackstate->cur_jack_state & JACK_IN) ?	"inserted" : "removed",
			(jackstate->cur_jack_state & JACK_4POLE) ? "inserted" : "removed");

	jackstate->btn_adc = jackstate->mdet_adc;

	mutex_unlock(&jackdet->mdet_lock);
	wake_unlock(&jackdet->jack_wake_lock);
}

static int return_gdet_adc_thd(struct aud3003x_jack *jackdet)
{
	struct earjack_state *jackstate = &jackdet->jack_state;

	/* Detect the type of inserted jack using gdet adc */
	if (jackstate->gdet_adc < jackdet->adc_thd_fake_jack)
		return JACK_RET_JACK;
	else if (jackstate->gdet_adc >= jackdet->adc_thd_fake_jack &&
			jackstate->gdet_adc < jackdet->adc_thd_auxcable)
		return JACK_RET_FAKE;
	else if (jackstate->gdet_adc >= jackdet->adc_thd_auxcable &&
			jackstate->gdet_adc < jackdet->adc_thd_water_in)
#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
		return JACK_RET_JACK;
#else
		return JACK_RET_AUX;
#endif
	else if (jackstate->gdet_adc >= jackdet->adc_thd_water_in &&
			jackstate->gdet_adc < jackdet->adc_thd_water_out)
		return JACK_RET_WATER;
	else if (jackstate->gdet_adc >= jackdet->adc_thd_water_out)
		return JACK_RET_OUT;
	else
		return JACK_RET_ERR;
}

/*
 * read_gdet_adc() - Get gdet adc data
 *
 * @jackdet: jackdet information struct
 *
 * Desc: Read gdet adc value.
 */
static void read_gdet_adc(struct aud3003x_jack *jackdet)
{
	struct aud3003x_priv *aud3003x = jackdet->p_aud3003x;
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct snd_soc_component *codec = jackdet->codec;

	regcache_cache_switch(aud3003x, false);

	/* GDET Pull-up registance 500k */
	i2c_client_change(aud3003x, AUD3003D);
	snd_soc_component_update_bits(codec, AUD3003X_D1_DCTR_TEST1,
			T_CTRV_GDET_POP_MASK, T_CTRV_GDET_POP_MASK);
	snd_soc_component_write(codec, AUD3003X_C0_ACTR_JD1, 0x72);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	jackstate->gdet_adc = adc_get_value(jackdet, 0);

	/* GDET Pull-up registance 2M */
	i2c_client_change(aud3003x, AUD3003D);
	snd_soc_component_write(codec, AUD3003X_C0_ACTR_JD1, 0x76);
	snd_soc_component_update_bits(codec, AUD3003X_D1_DCTR_TEST1,
			T_CTRV_GDET_POP_MASK, 0);
	i2c_client_change(aud3003x, CODEC_CLOSE);

	regcache_cache_switch(aud3003x, true);
}

/*
 * aud3003x_gdet_adc_work() - Process AP check interrupt
 *
 * @work: delayed workqueue
 *
 * Desc: When AP check interrupt is occured, need to check gdet adc value for
 * determine if it is jack or water. If it is jack, call jack_det_work process.
 * If it is water, set water polling configuration.
 */
static void aud3003x_gdet_adc_work(struct work_struct *work)
{
	struct aud3003x_jack *jackdet =
		container_of(work, struct aud3003x_jack, gdet_adc_work.work);
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_aud3003x->dev;

	mutex_lock(&jackdet->gdet_lock);

	/* read gdet adc value */
	read_gdet_adc(jackdet);

	dev_dbg(dev, "%s called, gdet adc: %d\n", __func__, jackstate->gdet_adc);

	switch (return_gdet_adc_thd(jackdet)) {
	case JACK_RET_JACK:
		/* Jack state: pole value decision */
		aud3003x_jackstate_set(jackdet, JACK_POLE_DEC);
		break;
#ifndef CONFIG_SND_SOC_AUD3003X_EXT_ANT
	case JACK_RET_AUX:
		/* Jack state: auxcable */
		aud3003x_jackstate_set(jackdet, JACK_AUX);
		break;
#endif
	case JACK_RET_FAKE:
		queue_delayed_work(jackdet->gdet_adc_wq, &jackdet->gdet_adc_work,
				msecs_to_jiffies(jackdet->fake_insert_retry_delay));
		break;
	case JACK_RET_WATER:
		/* Jack state: water detection */
		aud3003x_jackstate_set(jackdet, JACK_WATER);
		break;
	case JACK_RET_OUT:
		/* Jack state: water or jack out */
		aud3003x_jackstate_set(jackdet, JACK_OUT);
		break;
	default:
		break;
	}

	mutex_unlock(&jackdet->gdet_lock);
}

/*
 * aud3003x_configure_mic_bias() - Configure mic bias voltage
 *
 * @jackdet: jack information struct
 *
 * Desc: Configure the mic1 and mic2 bias voltages with default value
 * or the value received from the device tree.
 * Also configure the internal LDO voltage.
 */
void aud3003x_configure_mic_bias(struct aud3003x_jack *jackdet)
{
	struct snd_soc_component *codec = jackdet->codec;
	struct aud3003x_priv *aud3003x = jackdet->p_aud3003x;

	i2c_client_change(aud3003x, AUD3003D);

	/* Configure Mic1 Bias Voltage */
	snd_soc_component_update_bits(codec, AUD3003X_C3_ACTR_MCB1,
			CTRV_MCB1_NS_MASK,
			(jackdet->mic_bias1_voltage << CTRV_MCB1_NS_SHIFT));

	/* Configure Mic2 Bias Voltage */
	snd_soc_component_update_bits(codec, AUD3003X_C3_ACTR_MCB1,
			CTRV_MCB2_NS_MASK,
			(jackdet->mic_bias2_voltage << CTRV_MCB2_NS_SHIFT));

	/* Configure Mic3 Bias Voltage */
	snd_soc_component_update_bits(codec, AUD3003X_C4_ACTR_MCB2,
			CTRV_MCB3_NS_MASK,
			(jackdet->mic_bias3_voltage << CTRV_MCB3_NS_SHIFT));

	i2c_client_change(aud3003x, CODEC_CLOSE);
}

/*
 * aud3003x_register_inputdev() - Register button input device
 *
 * @jackdet: jack information struct
 *
 * Desc: Register button input device
 */
static int aud3003x_register_inputdev(struct aud3003x_jack *jackdet)
{
	struct device *dev = jackdet->p_aud3003x->dev;
	int i, ret;

	/* Register Headset Device */
	jackdet->input = devm_input_allocate_device(dev);
	if (!jackdet->input) {
		dev_err(dev, "Failed to allocate switch input device\n");
		return -ENOMEM;
	}

	jackdet->input->name = "AUD3003X Headset Input";
	jackdet->input->phys = dev_name(dev);
	jackdet->input->id.bustype = BUS_I2C;

	ret = input_register_device(jackdet->input);
	if (ret != 0) {
		jackdet->input = NULL;
		dev_err(dev, "Failed to register switch input device\n");
	}

	/*
	 * input_set_capability (dev, type, code)
	 * @dev : input device
	 * @type : event type (EV_KEY, EV_SW, etc...)
	 * @code : event code (4POLE, 3POLE, LINE...)
	 */
	/* 3-Pole event */
	input_set_capability(jackdet->input, EV_SW, SW_HEADPHONE_INSERT);
	/* 4-Pole event */
	input_set_capability(jackdet->input, EV_SW, SW_MICROPHONE_INSERT);
	/* Button event */
	for (i = 0; i < 4; i++)
		input_set_capability(jackdet->input, EV_KEY,
				jackdet->jack_buttons_zones[i].code);

	return 0;
}

/*
 * aud3003x_jack_parse_dt() - Parsing device tree options about jack
 *
 * @aud3003x: codec information struct
 *
 * Desc: Initialize jack_det struct variable as parsing device tree options.
 * Customer can tune this options. If not set by customer,
 * it use default value below defined.
 */

#define AUD3003X_GDET_DELAY					20
#define AUD3003X_MDET_DELAY					100
#define AUD3003X_MIC_ADC_DEFAULT			800
#define AUD3003X_BTN_ADC_DELAY				15
#define AUD3003X_BTN_REL_DEFAULT			1100
#define AUD3003X_ADC_THD_FAKE_JACK			15
#define AUD3003X_ADC_THD_AUXCABLE			90
#define AUD3003X_ADC_THD_WATER_IN			120
#define AUD3003X_ADC_THD_WATER_OUT			3280

#define AUD3003X_JACKIN_DBNC_DEFAULT		0xA0
#define AUD3003X_JACKOUT_DBNC_DEFAULT		0x02

static void aud3003x_jack_parse_dt(struct aud3003x_priv *aud3003x)
{
	struct aud3003x_jack *jackdet = aud3003x->p_jackdet;
	struct device *dev = aud3003x->dev;
	struct of_phandle_args args;
	int bias_v_conf, mic_range, btn_rel_val, delay;
	int jackin_dbnc, jackin_irq_dbnc, jackout_dbnc, jackout_irq_dbnc;
	int thd_adc;
	int i, ret;

	/*
	 * Set mic bias 1/2/3 voltages
	 */
	/* Mic Bias 1 */
	ret = of_property_read_u32(dev->of_node, "mic-bias1-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MIC_BIAS1_VO_2_6V) &&
			 (bias_v_conf <= MIC_BIAS1_VO_2_8V))) {
		jackdet->mic_bias1_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias1_voltage = MIC_BIAS1_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias1-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	/* Mic Bias 2 */
	ret = of_property_read_u32(dev->of_node, "mic-bias2-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MIC_BIAS2_VO_0_7V) &&
			 (bias_v_conf <= MIC_BIAS2_VO_2_8V))) {
		jackdet->mic_bias2_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias2_voltage = MIC_BIAS2_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias2-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	/* Mic Bias 3 */
	ret = of_property_read_u32(dev->of_node, "mic-bias3-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MIC_BIAS3_VO_2_6V) &&
			 (bias_v_conf <= MIC_BIAS3_VO_2_8V))) {
		jackdet->mic_bias3_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias3_voltage = MIC_BIAS3_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias3-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	dev_dbg(dev, "Bias voltage values: bias1=%d, bias2=%d, bias3=%d\n",
			jackdet->mic_bias1_voltage,
			jackdet->mic_bias2_voltage,
			jackdet->mic_bias3_voltage);

	/*
	 * Set mic detect tuning values
	 */
	/* GDET adc check delay time */
	ret = of_property_read_u32(dev->of_node, "gdet-delay", &delay);
	if (!ret)
		jackdet->gdet_delay = delay;
	else
		jackdet->gdet_delay = AUD3003X_GDET_DELAY;

	/* Mic bias on delay time */
	ret = of_property_read_u32(dev->of_node, "mic-det-delay", &delay);
	if (!ret)
		jackdet->mdet_delay = delay;
	else
		jackdet->mdet_delay = AUD3003X_MDET_DELAY;

	/* Mic det adc range */
	ret = of_property_read_u32(dev->of_node, "mic-adc-range", &mic_range);
	if (!ret)
		jackdet->mic_adc_range = mic_range;
	else
		jackdet->mic_adc_range = AUD3003X_MIC_ADC_DEFAULT;

	dev_dbg(dev, "GDET delay: %d, MDET delay: %d, Mic adc range: %d\n",
			jackdet->gdet_delay, jackdet->mdet_delay, jackdet->mic_adc_range);

	/*
	 * Jack Detect Tuning Values
	 */
	/* Jack-in debounce time */
	ret = of_property_read_u32(dev->of_node, "jackin-dbnc-time", &jackin_dbnc);
	if (!ret)
		jackdet->jackin_dbnc_time = A2D_JACK_DBNC_IN_MASK & (jackin_dbnc << A2D_JACK_DBNC_IN_SHIFT);
	else
		jackdet->jackin_dbnc_time = AUD3003X_JACKIN_DBNC_DEFAULT;

	ret = of_property_read_u32(dev->of_node, "jackin-irq-dbnc-time", &jackin_irq_dbnc);
	if (!ret)
		jackdet->jackin_irq_dbnc_time = A2D_JACK_DBNC_INT_IN_MASK & (jackin_irq_dbnc << A2D_JACK_DBNC_INT_IN_SHIFT);
	else
		jackdet->jackin_irq_dbnc_time = AUD3003X_JACKIN_DBNC_DEFAULT;

	/* Jack-out debounce time */
	ret = of_property_read_u32(dev->of_node, "jackout-dbnc-time", &jackout_dbnc);
	if (!ret)
		jackdet->jackout_dbnc_time = A2D_JACK_DBNC_OUT_MASK & jackout_dbnc;
	else
		jackdet->jackout_dbnc_time = AUD3003X_JACKOUT_DBNC_DEFAULT;

	/* Jack-out debounce time */
	ret = of_property_read_u32(dev->of_node, "jackout-irq-dbnc-time", &jackout_irq_dbnc);
	if (!ret)
		jackdet->jackout_irq_dbnc_time = A2D_JACK_DBNC_INT_OUT_MASK & jackout_irq_dbnc;
	else
		jackdet->jackout_irq_dbnc_time = AUD3003X_JACKOUT_DBNC_DEFAULT;

	dev_dbg(dev, "Jack Debounce time: 0x%02x, Jack IRQ Debounce time: 0x%02x\n",
			jackdet->jackin_dbnc_time | jackdet->jackout_dbnc_time,
			jackdet->jackin_irq_dbnc_time | jackdet->jackout_irq_dbnc_time);

	/*
	 * Set button adc tuning values
	 */
	/* Button adc check delay time */
	ret = of_property_read_u32(dev->of_node, "btn-adc-delay", &delay);
	if (!ret)
		jackdet->btn_adc_delay = delay;
	else
		jackdet->btn_adc_delay = AUD3003X_BTN_ADC_DELAY;

	/* Button press adc value, a maximum of 4 buttons are supported */
	for (i = 0; i < 4; i++) {
		if (of_parse_phandle_with_args(dev->of_node,
					"but-zones-list", "#list-but-cells", i, &args))
			break;
		jackdet->jack_buttons_zones[i].code = args.args[0];
		jackdet->jack_buttons_zones[i].adc_low = args.args[1];
		jackdet->jack_buttons_zones[i].adc_high = args.args[2];
	}

	/* Button release adc value */
	ret = of_property_read_u32(dev->of_node, "btn-release-value", &btn_rel_val);
	if (!ret)
		jackdet->btn_release_value = btn_rel_val;
	else
		jackdet->btn_release_value = AUD3003X_BTN_REL_DEFAULT;

	dev_dbg(dev, "Button adc check delay: %d\n",
			jackdet->btn_adc_delay);
	for (i = 0; i < 4; i++)
		dev_dbg(dev, "Button Press: code(%d), low(%d), high(%d)\n",
				jackdet->jack_buttons_zones[i].code,
				jackdet->jack_buttons_zones[i].adc_low,
				jackdet->jack_buttons_zones[i].adc_high);
	dev_dbg(dev, "Button Release: %d\n", jackdet->btn_release_value);

	/* Set gdet adc threshold value */
	ret = of_property_read_u32(dev->of_node, "adc-thd-fake-jack", &thd_adc);
	if (!ret)
		jackdet->adc_thd_fake_jack = thd_adc;
	else
		jackdet->adc_thd_fake_jack = AUD3003X_ADC_THD_FAKE_JACK;

	ret = of_property_read_u32(dev->of_node, "adc-thd-auxcable", &thd_adc);
	if (!ret)
		jackdet->adc_thd_auxcable = thd_adc;
	else
		jackdet->adc_thd_auxcable = AUD3003X_ADC_THD_AUXCABLE;

	ret = of_property_read_u32(dev->of_node, "adc-thd-water-in", &thd_adc);
	if (!ret)
		jackdet->adc_thd_water_in = thd_adc;
	else
		jackdet->adc_thd_water_in = AUD3003X_ADC_THD_WATER_IN;

	ret = of_property_read_u32(dev->of_node, "adc-thd-water-out", &thd_adc);
	if (!ret)
		jackdet->adc_thd_water_out = thd_adc;
	else
		jackdet->adc_thd_water_out = AUD3003X_ADC_THD_WATER_OUT;

	dev_dbg(dev, "GDET adc threshold value: %d %d %d %d\n",
			jackdet->adc_thd_fake_jack,
			jackdet->adc_thd_auxcable,
			jackdet->adc_thd_water_in,
			jackdet->adc_thd_water_out);
}

static void aud3003x_jack_register_initialize(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	struct aud3003x_jack *jackdet = aud3003x->p_jackdet;

#ifdef CONFIG_PM
	pm_runtime_get_sync(codec->dev);
#else
	aud3003x_enable(codec->dev);
#endif

	dev_dbg(codec->dev, "%s called, setting defaults\n", __func__);

	i2c_client_change(aud3003x, AUD3003D);

	/* Jack HMU On basic value */
	snd_soc_component_write(codec, AUD3003X_B7_ODSEL8, 0x08);
	snd_soc_component_write(codec, AUD3003X_B8_ODSEL9, 0x02);
	snd_soc_component_write(codec, AUD3003X_C0_ACTR_JD1, 0x76);
	snd_soc_component_write(codec, AUD3003X_C1_ACTR_JD2, 0x07);
	snd_soc_component_write(codec, AUD3003X_CA_ACTR_IMP4, 0x67);
	snd_soc_component_write(codec, AUD3003X_D4_DCTR_TEST4, 0x04);
	snd_soc_component_write(codec, AUD3003X_DA_DCTR_DBNC3, 0xC0);
	snd_soc_component_write(codec, AUD3003X_E9_DCTR_IMP3, 0x82);

	/* IRQ Un-Masking */
#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
	snd_soc_component_write(codec, AUD3003X_08_IRQ1M, 0x00);
	snd_soc_component_write(codec, AUD3003X_09_IRQ2M, 0x00);
	snd_soc_component_write(codec, AUD3003X_0A_IRQ3M, 0x0E);
	snd_soc_component_write(codec, AUD3003X_0B_IRQ4M, 0x0E);
#else
	snd_soc_component_write(codec, AUD3003X_08_IRQ1M, 0x02);
	snd_soc_component_write(codec, AUD3003X_09_IRQ2M, 0x02);
	snd_soc_component_write(codec, AUD3003X_0A_IRQ3M, 0xCE);
	snd_soc_component_write(codec, AUD3003X_0B_IRQ4M, 0xCE);
#endif
	snd_soc_component_write(codec, AUD3003X_0C_IRQ5M, 0x03);
	snd_soc_component_write(codec, AUD3003X_0D_IRQ6M, 0x03);

	/* Jack Clock initialize */
	snd_soc_component_update_bits(codec, AUD3003X_D1_DCTR_TEST1,
			T_SEL_CLK_DET_MASK, T_SEL_CLK_DET_MASK);

	/* Pin Mode */
	snd_soc_component_write(codec, AUD3003X_D0_DCTR_CM, 0x23);

	/* ANT MDET Masking */
	snd_soc_component_update_bits(codec, AUD3003X_D2_DCTR_TEST2,
			T_PDB_ANT_MDET_MASK, T_PDB_ANT_MDET_MASK);

	/* IMP Skip off */
	snd_soc_component_update_bits(codec, AUD3003X_EA_DCTR_IMP4,
			IMP_SKIP_OPT_MASK, 0);

	/* Jack in/out debounce time */
	snd_soc_component_write(codec, AUD3003X_D8_DCTR_DBNC1,
			jackdet->jackin_dbnc_time | jackdet->jackout_dbnc_time);
	/* Jack in/out irq debounce time */
	snd_soc_component_write(codec, AUD3003X_D9_DCTR_DBNC2,
			jackdet->jackin_irq_dbnc_time | jackdet->jackout_irq_dbnc_time);

	i2c_client_change(aud3003x, CODEC_CLOSE);
	i2c_client_change(aud3003x, AUD3003A);

	/* JACK EN_OUTTIEL, EN_OUTTIER */
	snd_soc_component_write(codec, AUD3003X_13A_POP_HP, 0x70);

	/* OVP MUTE SEL */
	snd_soc_component_update_bits(codec, AUD3003X_142_CTRL_OVP0,
			OVP_MUTE_SEL_MASK, OVP_MUTE_SEL_MASK);

	i2c_client_change(aud3003x, CODEC_CLOSE);

	/* ADC OTP value write */
	aud3003x_acpm_write_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F63_WTP_ADC1, 0xC8);
	aud3003x_acpm_write_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F64_WTP_ADC2, 0x2E);
	aud3003x_acpm_write_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F65_WTP_ADC3, 0x09);
	aud3003x_acpm_write_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F67_OTP_ADC2, 0xC8);
	aud3003x_acpm_write_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F68_OTP_ADC3, 0x2E);
	aud3003x_acpm_write_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F69_OTP_ADC4, 0x09);
	aud3003x_acpm_write_reg(AUD3003X_CLOSE_ADDR, AUD3003X_F6A_OTP_ADC5, 0x77);

	/* All boot time hardware access is done. Put the device to sleep. */
#ifdef CONFIG_PM
	pm_runtime_put_sync(codec->dev);
#else
	aud3003x_disable(codec->dev);
#endif
}

static void aud3003x_jack_register_exit(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);

	/* Slave PMIC CODEC IRQ Masking */
	aud3003x_acpm_update_reg(AUD3003X_COMMON_ADDR,
			AUD3003X_007_IRQM, CDC_IRQM_MASK, CDC_IRQM_MASK);

	i2c_client_change(aud3003x, AUD3003D);

	/* IRQ Masking */
	snd_soc_component_write(codec, AUD3003X_08_IRQ1M, 0xFF);
	snd_soc_component_write(codec, AUD3003X_09_IRQ2M, 0xFF);
	snd_soc_component_write(codec, AUD3003X_0A_IRQ3M, 0xFF);
	snd_soc_component_write(codec, AUD3003X_0B_IRQ4M, 0xFF);
	snd_soc_component_write(codec, AUD3003X_0C_IRQ5M, 0xFF);
	snd_soc_component_write(codec, AUD3003X_0D_IRQ6M, 0xFF);

	i2c_client_change(aud3003x, CODEC_CLOSE);
} 

struct codec_notifier_struct {
	struct aud3003x_priv *aud3003x;
};
static struct codec_notifier_struct codec_notifier_t;

static int return_irq_type(struct aud3003x_jack *jackdet)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	unsigned int pend1, pend2, pend3, pend4, pend5, pend6, stat1, stat2;

	pend1 = jackdet->irq_val[0];
	pend2 = jackdet->irq_val[1];
	pend3 = jackdet->irq_val[2];
	pend4 = jackdet->irq_val[3];
	pend5 = jackdet->irq_val[4];
	pend6 = jackdet->irq_val[5];
	stat1 = jackdet->irq_val[6];
	stat2 = jackdet->irq_val[7];

	if (pend5 & (OVP_MUTER_R | OVP_MUTEL_R))
		return IRQ_ST_OVP_IN;
	if (pend6 & (OVP_MUTER_F | OVP_MUTEL_F))
		return IRQ_ST_OVP_OUT;

	if (jackstate->irq_masking)
		return IRQ_ST_MASKING;

#ifdef CONFIG_SND_SOC_AUD3003X_EXT_ANT
	if ((pend1 & ST_APCHECK_R) || (pend2 & ST_MOIST_F))
		return IRQ_ST_APCHECK;
	else if (pend3 & ST_POLE_R)
		return IRQ_ST_JACKDET;
	else if (pend3 & IMP_CHECK_DONE_R)
		return IRQ_ST_IMP_CHK;
	else if (pend1 & (ST_JACKOUT_R | ST_ANT_JACKOUT_R))
		return IRQ_ST_JACKOUT;
	else if (pend1 & (ST_WTJACKIN_R | ST_WTJACKOUT_R))
		return IRQ_ST_WTJACK_IRQ;
	else if ((pend3 & BTN_DET_R) || (pend4 & BTN_DET_F))
		return IRQ_ST_BTN_DET;
	else
		return IRQ_ST_ERR;
#else
	if ((pend1 & ST_APCHECK_R) || (pend2 & ST_MOIST_F))
		return IRQ_ST_APCHECK;
	else if (pend3 & ST_POLE_R)
		return IRQ_ST_JACKDET;
	else if (pend3 & IMP_CHECK_DONE_R)
		return IRQ_ST_IMP_CHK;
	else if (pend1 & ST_JACKOUT_R)
		return IRQ_ST_JACKOUT;
	else if (pend1 & (ST_WTJACKIN_R | ST_WTJACKOUT_R))
		return IRQ_ST_WTJACK_IRQ;
	else if ((pend3 & BTN_DET_R) || (pend4 & BTN_DET_F))
		return IRQ_ST_BTN_DET;
	else
		return IRQ_ST_ERR;
#endif

	return 0;
}

/*
 * aud3003x_notifier_handler() - Codec IRQ Handler
 *
 * Desc: Set codec register according to codec IRQ.
 */
static int aud3003x_notifier_handler(struct notifier_block *nb,
		unsigned long insert, void *data)
{
	struct codec_notifier_struct *codec_notifier_data = data;
	struct aud3003x_priv *aud3003x = codec_notifier_data->aud3003x;
	struct aud3003x_jack *jackdet = aud3003x->p_jackdet;
	struct earjack_state *jackstate = &jackdet->jack_state;
//	struct snd_soc_component *codec = jackdet->codec;
	struct device *dev = aud3003x->dev;

	mutex_lock(&jackdet->key_lock);

	switch (return_irq_type(jackdet)) {
	case IRQ_ST_MASKING:
		dev_dbg(dev, "[IRQ] %s IRQ masking due to OVP, line %d\n",
				__func__, __LINE__);
		break;
	case IRQ_ST_OVP_IN:
		dev_dbg(dev, "[IRQ] %s OVP in, line %d\n", __func__, __LINE__);
		jackstate->irq_masking = true;
		break;
	case IRQ_ST_OVP_OUT:
		dev_dbg(dev, "[IRQ] %s OVP out, line %d\n", __func__, __LINE__);
		jackstate->irq_masking = false;

		aud3003x_jackstate_set(jackdet, JACK_POLE_DEC);

		/* Wake lock for jack and button irq */
		wake_lock(&jackdet->jack_wake_lock);

		queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
				msecs_to_jiffies(jackdet->mdet_delay));
		break;
	case IRQ_ST_APCHECK:
		dev_dbg(dev, "[IRQ] %s AP Check state, line: %d\n",
				__func__, __LINE__);

		aud3003x_jackstate_set(jackdet, JACK_APCHK);

		queue_delayed_work(jackdet->gdet_adc_wq, &jackdet->gdet_adc_work,
				msecs_to_jiffies(jackdet->gdet_delay));
		break;
	case IRQ_ST_IMP_CHK:
		dev_dbg(dev, "[IRQ] %s Jack impedance check, line: %d\n",
				__func__, __LINE__);
		/* Call impedance calculator */
		break;
	case IRQ_ST_JACKDET:
		dev_dbg(dev, "[IRQ] %s Jack detected, read mic adc, line: %d\n",
				__func__, __LINE__);

		/* Wake lock for jack and button irq */
		wake_lock(&jackdet->jack_wake_lock);

		queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
				msecs_to_jiffies(0));
		break;
	case IRQ_ST_JACKOUT:
		dev_dbg(dev, "[IRQ] %s Jack out interrupt, line: %d\n",
				__func__, __LINE__);

		aud3003x_jackstate_set(jackdet, JACK_OUT);

		queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
				msecs_to_jiffies(0));

		queue_delayed_work(jackdet->buttons_wq, &jackdet->buttons_work,
				msecs_to_jiffies(jackdet->btn_adc_delay));
		break;
	case IRQ_ST_WTJACK_IRQ:
		dev_dbg(dev, "[IRQ] %s Jack interrupt in water, line: %d\n",
				__func__, __LINE__);

		aud3003x_jackstate_set(jackdet, JACK_WATER_OUT);
		break;
	case IRQ_ST_BTN_DET:
		dev_dbg(dev, "[IRQ] %s Button interrupt, line: %d\n",
				__func__, __LINE__);

		/* Wake lock for jack and button irq */
		wake_lock(&jackdet->jack_wake_lock);

		queue_delayed_work(jackdet->buttons_wq, &jackdet->buttons_work,
				msecs_to_jiffies(jackdet->btn_adc_delay));
		break;
	default:
		dev_dbg(dev, "[IRQ] %s IRQ return type skip, line %d\n",
				__func__, __LINE__);
		break;
	}

	mutex_unlock(&jackdet->key_lock);
	return IRQ_HANDLED;
}
static BLOCKING_NOTIFIER_HEAD(aud3003x_notifier);

int aud3003x_register_notifier(struct notifier_block *n,
		struct aud3003x_priv *aud3003x)
{
	int ret;

	codec_notifier_t.aud3003x = aud3003x;
	ret = blocking_notifier_chain_register(&aud3003x_notifier, n);
	if (ret < 0)
		pr_err("[IRQ] %s(%d)\n", __func__, __LINE__);
	return ret;
}

/*
 * aud3003x_call_notifier() - Notifier registration
 *
 * Desc: MFD driver get interrupts from PMIC and MFD filters the interrups.
 * If the interrup belongs to codec, then it notify the interrupt to the codec.
 * The notifier is the way to communicate between them.
 */
void aud3003x_call_notifier()
{
	struct aud3003x_priv *aud3003x = codec_notifier_t.aud3003x;
	struct aud3003x_jack *jackdet = aud3003x->p_jackdet;

	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_01_IRQ1PEND, &jackdet->irq_val[0]);
	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_02_IRQ2PEND, &jackdet->irq_val[1]);
	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_03_IRQ3PEND, &jackdet->irq_val[2]);
	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_04_IRQ4PEND, &jackdet->irq_val[3]);
	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_05_IRQ5PEND, &jackdet->irq_val[4]);
	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_06_IRQ6PEND, &jackdet->irq_val[5]);
	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_F0_STATUS1, &jackdet->irq_val[6]);
	aud3003x_acpm_read_reg(AUD3003X_DIGITAL_ADDR,
			AUD3003X_F1_STATUS2, &jackdet->irq_val[7]);

	dev_dbg(aud3003x->dev,
			"[IRQ] %s(%d) 0x1:%02x 0x2:%02x 0x3:%02x 0x4:%02x 0x5:%02x 0x6:%02x st1:%02x st2:%02x\n",
			__func__, __LINE__, jackdet->irq_val[0], jackdet->irq_val[1],
			jackdet->irq_val[2], jackdet->irq_val[3], jackdet->irq_val[4],
			jackdet->irq_val[5], jackdet->irq_val[6], jackdet->irq_val[7]);

	blocking_notifier_call_chain(&aud3003x_notifier, 0, &codec_notifier_t);
}
EXPORT_SYMBOL(aud3003x_call_notifier);
struct notifier_block codec_notifier;

/*
 * aud3003x_jack_probe() - Initialize variable related jack
 *
 * @codec: SoC audio codec device
 *
 * Desc: This function is called by aud3003x_codec_probe. For separate codec
 * and jack code, this function called from codec driver. This function initialize
 * jack variable, workqueue, mutex, and wakelock.
 */
int aud3003x_jack_probe(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	struct aud3003x_jack *jackdet;
	struct earjack_state *jackstate;

	dev_dbg(codec->dev, "Codec Jack Probe: (%s)\n", __func__);

	jackdet = kzalloc(sizeof(struct aud3003x_jack), GFP_KERNEL);
	if (jackdet == NULL)
		return -ENOMEM;

	jackdet->codec = codec;
	jackdet->p_aud3003x = aud3003x;
	aud3003x->p_jackdet = jackdet;
	jackstate = &jackdet->jack_state;

	/* Initialize jack state variable */
	jackstate->prv_jack_state = JACK_OUT;
	jackstate->cur_jack_state = JACK_OUT;
	jackstate->gdet_adc = -EINVAL;
	jackstate->mdet_adc = -EINVAL;
	jackstate->prv_btn_state = BUTTON_RELEASE;
	jackstate->cur_btn_state = BUTTON_RELEASE;
	jackstate->button_code = -EINVAL;
	jackstate->btn_adc = -EINVAL;
	jackstate->irq_masking = false;

	/* Initialize workqueue */
	/* Initialize workqueue for button handling */
	INIT_DELAYED_WORK(&jackdet->buttons_work, aud3003x_buttons_work);
	jackdet->buttons_wq = create_singlethread_workqueue("buttons_wq");
	if (jackdet->buttons_wq == NULL) {
		dev_err(codec->dev, "Failed to create buttons_wq\n");
		return -ENOMEM;
	}

	/* Initiallize workqueue for jack detect handling */
	INIT_DELAYED_WORK(&jackdet->jack_det_work, aud3003x_jack_det_work);
	jackdet->jack_det_wq = create_singlethread_workqueue("jack_det_wq");
	if (jackdet->jack_det_wq == NULL) {
		dev_err(codec->dev, "Failed to create jack_det_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for water detect handling */
	INIT_DELAYED_WORK(&jackdet->gdet_adc_work, aud3003x_gdet_adc_work);
	jackdet->gdet_adc_wq = create_singlethread_workqueue("gdet_adc_wq");
	if (jackdet->gdet_adc_wq == NULL) {
		dev_err(codec->dev, "Failed to create gdet_adc_wq\n");
		return -ENOMEM;
	}

	/* Initialize wake lock */
	wake_lock_init(&jackdet->jack_wake_lock, WAKE_LOCK_SUSPEND, "jack_wl");

	/* Initialize mutex lock */
	mutex_init(&jackdet->key_lock);
	mutex_init(&jackdet->gdet_lock);
	mutex_init(&jackdet->mdet_lock);

	/* Register ADC pin */
	adc_start(jackdet);

	/*
	 * Interrupt pin should be shared with pmic.
	 * So codec driver use notifier because of knowing
	 * the interrupt status from mfd.
	 */
	codec_notifier.notifier_call = aud3003x_notifier_handler,
		aud3003x_register_notifier(&codec_notifier, aud3003x);
	set_codec_notifier_flag(true);

	/* Device Tree for jack */
	aud3003x_jack_parse_dt(aud3003x);
	/* Configure mic bias voltage */
	aud3003x_configure_mic_bias(jackdet);
	/* register input device */
	aud3003x_register_inputdev(jackdet);
	/* register value init for jack */
	aud3003x_jack_register_initialize(codec);
	/* Slave PMIC CODEC IRQ Un-Masking */
	aud3003x_acpm_update_reg(AUD3003X_COMMON_ADDR,
			AUD3003X_007_IRQM, 0, CDC_IRQM_MASK);

	dev_dbg(codec->dev, "Jack probe done.\n");

	return true;
}

/*
 * aud3003x_jack_remove() - Remove variable related jack
 *
 * @codec: SoC audio codec device
 *
 * Desc: This function is called by aud3003x_codec_remove. For separate codec
 * and jack code, this function called from codec driver. This function destroy
 * workqueue.
 */
int aud3003x_jack_remove(struct snd_soc_component *codec)
{
	struct aud3003x_priv *aud3003x = snd_soc_component_get_drvdata(codec);
	struct aud3003x_jack *jackdet = aud3003x->p_jackdet;

	dev_dbg(codec->dev, "(*) %s called\n", __func__);

	wake_lock_destroy(&jackdet->jack_wake_lock);

	destroy_workqueue(jackdet->buttons_wq);
	destroy_workqueue(jackdet->jack_det_wq);
	destroy_workqueue(jackdet->gdet_adc_wq);

	set_codec_notifier_flag(false);
	aud3003x_jack_register_exit(codec);

	/* Unregister ADC pin */
	adc_stop(jackdet);
	kfree(jackdet);

	return true;
}

