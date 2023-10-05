/*
 * sound/soc/codec/s5m3700x-jack.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5M3700X_JACK_H
#define _S5M3700X_JACK_H

/* Jack state flag */
#define JACK_OUT		BIT(0)
#define JACK_POLE_DEC	BIT(1)
#define JACK_3POLE		BIT(2)
#define JACK_4POLE		BIT(3)
#define JACK_AUX		BIT(4)
#define JACK_OMTP		BIT(5)
#define JACK_CMP		BIT(6)
#define JACK_IMP		BIT(7)
#define JACK_WTP_DEC	BIT(8)
#define JACK_WTP_JIO	BIT(9)
#define JACK_WTP_CJI	BIT(10)
#define JACK_WTP_ETC	BIT(11)
#define JACK_WTP_POLL	BIT(12)
#define JACK_WTP_JO		BIT(13)
#define JACK_IN			(BIT(2) | BIT(3) | BIT(4) | BIT(5))
#define JACK_WTP_ST		(BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(12) | BIT(13))

#define USB_OUT			BIT(16)
#define USB_INIT		BIT(17)
#define USB_JACK_CMP	BIT(18)
#define USB_JACK_IMP	BIT(19)
#define USB_POLE_DEC	BIT(20)
#define USB_3POLE		BIT(21)
#define USB_4POLE		BIT(22)
#define USB_AUX			BIT(23)
#define USB_MCHK_DONE	BIT(24)
#define USB_IN			(BIT(21) | BIT(22) | BIT(23))
#define USB_INSERTING	(BIT(17) | BIT(18) | BIT(19) | BIT(20) | BIT(24))


/* USB HP Offset Cal Default */
#define S5M3700X_USB_HP_CHL_OFFSET			100
#define S5M3700X_USB_HP_CHR_OFFSET			100

/* NUM_JACK_IRQ */
#define S5M3700X_NUM_JACK_IRQ				10

/* Workqueue Delay */
#define MDET_CHK_DELAY						20
#define LDET_CHK_DELAY						0
#define JACK_MDET_DELAY						50
#define BTN_ADC_DELAY						0

/* Threshold for Water Decision */
#define S5M3700X_WTP_THD_DEFAULT			-1
#define S5M3700X_WTP_THD_LOW_RANGE			0
#define S5M3700X_WTP_THD_MID_RANGE			1
#define S5M3700X_WTP_THD_HIGH_RANGE			2

/* default range for mic adc */
#define S5M3700X_MIC_ADC_RANGE_0			1120
#define S5M3700X_MIC_ADC_RANGE_1			3447

/* Left Impedance */
#define S5M3700X_JACK_IMP_RANGE_0			0
#define S5M3700X_JACK_IMP_RANGE_1			15
#define S5M3700X_JACK_IMP_RANGE_2			32
#define S5M3700X_JACK_IMP_RANGE_3			50
#define S5M3700X_JACK_IMP_RANGE_4			110

/* OMTP Default (Not Support) */
#define S5M3700X_OMTP_ADC_DEFAULT			-1

/* JACK MASK */
#define S5M3700X_JACK_MASK			(SND_JACK_HEADPHONE|SND_JACK_HEADSET| \
									SND_JACK_LINEOUT)

/* USBJACK MASK */
#define S5M3700X_USBJACK_MASK		(SND_JACK_HEADPHONE|SND_JACK_HEADSET)

/* BUTTON MASK */
#define S5M3700X_BUTTON_MASK		(SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
									SND_JACK_BTN_2 | SND_JACK_BTN_3)

enum {
	JACK_IMP_VAL_1 = 0,
	JACK_IMP_VAL_2,
	JACK_IMP_VAL_3,
	JACK_IMP_VAL_4,
	JACK_IMP_VAL_5,
};

/* Handler Argument for CCIC event */
enum {
	JACK_HANDLER_EVENT_NONE = 0,
	JACK_HANDLER_EVENT_JACKIN = 1,
	JACK_HANDLER_EVENT_JACKOUT = 2,
};

/* USB Gender Connection State */
enum {
	USB_DIRECT_CONNECTION = 0,
	USB_FLIP_CONNECTION = 1,
};

#if IS_ENABLED(CONFIG_USB_SW_MAX20328B)
enum max_function {
	MAX_MIC_GND_SWAP,
	MAX_GND_MIC_SWAP,
	MAX_USBC_AUDIO_HP_ON,
	MAX_USBC_AUDIO_HP_OFF,
	MAX_USBC_ORIENTATION_CC1,
	MAX_USBC_ORIENTATION_CC2,
	MAX_USBC_DISPLAYPORT_DISCONNECTED,
	MAX_USBC_FAST_CHARGE_SELECT,
	MAX_USBC_FAST_CHARGE_EXIT,
	MAX_USBC_SWITCH_ENABLE,
	MAX_USBC_SWITCH_DISABLE,
	MAX_USBC_SWITCH_SBU_DIRECT_CONNECT,
	MAX_USBC_SWITCH_SBU_FLIP_CONNECT,
	MAX_USBC_SWITCH_SBU_HIZ,
	MAX_EVENT_MAX,
};
#endif

/* IRQ return type */
enum {
	IRQ_ST_CMP_JACK_IN = 0,
	IRQ_ST_IMP_CHK,
	IRQ_ST_JACKDET,
	IRQ_ST_JACKOUT,
	IRQ_ST_WTJACK_DEC,
	IRQ_ST_WTJACK_IN,
	IRQ_ST_WTJACK_OUT,
	IRQ_ST_WTJACK_ETC,
	IRQ_ST_BTN_RELEASE,
	IRQ_ST_BTN_PRESS,
	IRQ_ST_JACKIN,
	IRQ_ST_ERR,
};

/* ADC type */
enum {
	ADC_POLE = 0,
	ADC_BTN,
	ADC_IMP,
	ADC_WTP,
};

/* ADC PIN Selection */
enum {
	JACK_LDET_ADC = 0,
	JACK_GDET_ADC = 1,
	JACK_MDET_ADC = 2,
	JACK_IMP_ADC = 3,
	JACK_WTP_LDET_ADC = 4,
};

/* Button state */
enum {
	BUTTON_RELEASE = 0,
	BUTTON_PRESS,
};

struct s5m3700x_irq_type_map {
	int addr_index;
	int irq;
	int type;
};

/* Button range */
struct jack_btn_zone {
	int adc_low;
	int adc_high;
};

struct s5m3700x_priv;

/* Jack information struct */
struct s5m3700x_jack {
	/* Codec Driver default */
	struct snd_soc_component *codec;
	struct s5m3700x_priv *p_s5m3700x;

	/* IRQ */
	int irqb_gpio;
	int codec_irq;
	u8 irq_val[S5M3700X_NUM_JACK_IRQ];

	/* jack device */
	struct snd_soc_jack headset_jack;

	/* jack device */
	struct snd_soc_jack headset_button;

	/* jack detection notifier */
	struct notifier_block s5m3700x_jack_det_nb;

	/* mutex */
	struct mutex key_lock;
	struct mutex adc_lock;
	struct mutex jack_det_lock;
	struct wakeup_source *jack_wakeup;

	/* Workqueue */
	struct delayed_work mdet_chk_work;
	struct workqueue_struct *mdet_chk_wq;

	struct delayed_work ldet_chk_work;
	struct workqueue_struct *ldet_chk_wq;

	struct delayed_work jack_det_work;
	struct workqueue_struct *jack_det_wq;

	struct delayed_work btn_release_work;
	struct workqueue_struct *btn_release_wq;

	struct delayed_work btn_press_work;
	struct workqueue_struct *btn_press_wq;

	/* Impedance Calibration */
	int imp_p[3];

	/* wtp ldet thd */
	int wtp_ldet_thd[3];

	/* buttons zone */
	struct jack_btn_zone btn_zones[4];

	/* MicBias */
	int mic_bias1_voltage;
	int mic_bias2_voltage;
	int mic_bias3_voltage;

	/* USB HP Offset Cal */
	int usb_hp_chl;
	int usb_hp_chr;

	/* status */
	bool is_probe_done;
	int prv_jack_state;
	int cur_jack_state;
	int btn_state;
	int adc_re_read;
	int switch_ic_state;
	int mic_adc_range[2];
	int jack_imp_range[5];
	int omtp_range;
};
/* probe/remove */
int s5m3700x_jack_probe(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x);
int s5m3700x_jack_remove(struct snd_soc_component *codec);
int s5m3700x_usbjack_probe(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x);
int s5m3700x_usbjack_remove(struct snd_soc_component *codec);

/* common function */
void jack_wake_lock(struct wakeup_source *ws);
void jack_wake_unlock(struct wakeup_source *ws);
int jack_set_wake_lock(struct s5m3700x_jack *jackdet);
void jack_unregister_wake_lock(struct s5m3700x_jack *jackdet);
void reset_mic4_boost(struct s5m3700x_priv *s5m3700x);
int s5m3700x_get_irq_type(struct s5m3700x_priv *s5m3700x, unsigned int event);
void s5m3700x_register_jack_controls(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x);
void s5m3700x_jack_variable_initialize(struct s5m3700x_priv *s5m3700x, struct s5m3700x_jack *jackdet);
void s5m3700x_common_parse_dt(struct s5m3700x_priv *s5m3700x);
int s5m3700x_request_threaded_irq(struct s5m3700x_priv *s5m3700x,
		irqreturn_t (*s5m3700x_irq_thread)(int, void *));
int s5m3700x_register_notifier(struct s5m3700x_priv *s5m3700x,
		int (*s5m3700x_notifier_handler)(struct notifier_block *, unsigned long, void *));
int s5m3700x_register_jack(struct snd_soc_component *codec,
		struct s5m3700x_priv *s5m3700x, int type);
int s5m3700x_register_button(struct snd_soc_component *codec,
		struct s5m3700x_priv *s5m3700x, int type);
void s5m3700x_report_jack_type(struct s5m3700x_priv *s5m3700x, int type, int insert);
void s5m3700x_report_button_type(struct s5m3700x_priv *s5m3700x,
		int type, int state, int btn_adc);
bool s5m3700x_button_error_check(struct s5m3700x_priv *s5m3700x, int btn_irq_state);
void s5m3700x_call_notifier(struct s5m3700x_priv *s5m3700x, int insert);
void s5m3700x_read_irq(struct s5m3700x_priv *s5m3700x);
void s5m3700x_wtp_threshold_initialize(struct s5m3700x_priv *s5m3700x);
void s5m3700x_hp_trim_initialize(struct s5m3700x_priv *s5m3700x);
void s5m3700x_configure_mic_bias(struct s5m3700x_priv *s5m3700x);
void s5m3700x_read_imp_otp(struct s5m3700x_priv *s5m3700x);
void s5m3700x_jack_register_exit(struct snd_soc_component *codec);
bool s5m3700x_check_jack_state_sequence(struct s5m3700x_priv *s5m3700x);
char *s5m3700x_return_status_name(unsigned int status);
char *s5m3700x_print_jack_type_state(int cur_jack_state);
void s5m3700x_print_status_change(struct device *dev, unsigned int prv_jack, unsigned int cur_jack);

#endif /* _S5M3700X_JACK_H */
