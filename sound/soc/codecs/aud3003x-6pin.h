/*
 * sound/soc/codec/aud3003x-6pin.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _AUD3003X_5PIN_H
#define _AUD3003X_5PIN_H

/* Jack state flag */
#define JACK_OUT		BIT(0)
#define JACK_APCHK		BIT(1)
#define JACK_WATER		BIT(2)
#define JACK_WATER_OUT	BIT(3)

#define JACK_POLE_DEC	BIT(4)
#define JACK_3POLE		BIT(5)
#define JACK_4POLE		BIT(6)
#define JACK_IN			(BIT(6) | BIT(5))
#ifndef CONFIG_SND_SOC_AUD3003X_EXT_ANT
#define JACK_AUX		BIT(7)
#endif

/* GDET jack return type */
enum {
	JACK_RET_ERR = 0,
	JACK_RET_JACK,
#ifndef CONFIG_SND_SOC_AUD3003X_EXT_ANT
	JACK_RET_AUX,
#endif
	JACK_RET_FAKE,
	JACK_RET_WATER,
	JACK_RET_OUT,
};

/* Button state */
enum {
	BUTTON_RELEASE = 0,
	BUTTON_PRESS,
};

/* IRQ return type */
enum {
	IRQ_ST_MASKING = 0,
	IRQ_ST_OVP_IN,
	IRQ_ST_OVP_OUT,
	IRQ_ST_APCHECK,
	IRQ_ST_IMP_CHK,
	IRQ_ST_JACKDET,
	IRQ_ST_JACKOUT,
	IRQ_ST_WTJACK_IRQ,
	IRQ_ST_BTN_DET,
	IRQ_ST_ERR,
};

/* Jack state machine */
struct earjack_state {
	/* Jack */
	u8 prv_jack_state;
	u8 cur_jack_state;
	int gdet_adc;
	int mdet_adc;
	/* Button */
	bool prv_btn_state;
	bool cur_btn_state;
	int button_code;
	int btn_adc;
	/* Others */
	bool irq_masking;
};

/* Button range */
struct jack_buttons_zone {
	int code;
	int adc_low;
	int adc_high;
};

struct aud3003x_priv;

/* Jack information struct */
struct aud3003x_jack {
	/* jack driver default */
	struct snd_soc_component *codec;
	struct aud3003x_priv *p_aud3003x;
	struct input_dev *input;
	struct iio_channel *jack_adc_iio;
	unsigned int irq_val[8];
	struct wake_lock jack_wake_lock;
	struct mutex key_lock;
	struct mutex gdet_lock;
	struct mutex mdet_lock;
	/* jack workqueue */
	struct delayed_work buttons_work;
	struct workqueue_struct *buttons_wq;
	struct delayed_work jack_det_work;
	struct workqueue_struct *jack_det_wq;
	struct delayed_work gdet_adc_work;
	struct workqueue_struct *gdet_adc_wq;
	/* jack parse dt */
	int int_gpio;
	int mic_bias1_voltage;
	int mic_bias2_voltage;
	int mic_bias3_voltage;
	int gdet_delay;
	int mdet_delay;
	int fake_insert_retry_delay;
	int mic_adc_range;
	int btn_adc_delay;
	struct jack_buttons_zone jack_buttons_zones[4];
	int btn_release_value;
	unsigned int jackin_dbnc_time;
	unsigned int jackin_irq_dbnc_time;
	unsigned int jackout_dbnc_time;
	unsigned int jackout_irq_dbnc_time;
	/* gdet adc thd */
	int adc_thd_fake_jack;
	int adc_thd_auxcable;
	int adc_thd_water_in;
	int adc_thd_water_out;
	/* jack state machine */
	struct earjack_state jack_state;
};

#endif /* _AUD3003X_5PIN_H */

