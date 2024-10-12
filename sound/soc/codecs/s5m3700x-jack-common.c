/*
 * sound/soc/codec/s5m3700x-jack-common.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input-event-codes.h>

#include "s5m3700x.h"
#include "s5m3700x-jack.h"
#include "s5m3700x-register.h"

/* Jack Detection Notifier Head */
static BLOCKING_NOTIFIER_HEAD(s5m3700x_notifier);

/* JACK IRQ */
unsigned int s5m3700x_jack_irq_addr[] = {
	S5M3700X_001_IRQ1,
	S5M3700X_002_IRQ2,
	S5M3700X_003_IRQ3,
	S5M3700X_004_IRQ4,
	S5M3700X_005_IRQ5,
	S5M3700X_006_IRQ6,
	S5M3700X_0F0_STATUS1,
	S5M3700X_0F1_STATUS2,
	S5M3700X_0F2_STATUS3,
	S5M3700X_0FA_STATUS11
};

/* IRQ Address Index*/
enum {
	S5M3700X_001_IRQ1_INDEX = 0,
	S5M3700X_002_IRQ2_INDEX,
	S5M3700X_003_IRQ3_INDEX,
	S5M3700X_004_IRQ4_INDEX,
	S5M3700X_005_IRQ5_INDEX,
	S5M3700X_006_IRQ6_INDEX,
	S5M3700X_0F0_STATUS1_INDEX,
	S5M3700X_0F1_STATUS2_INDEX,
	S5M3700X_0F2_STATUS3_INDEX,
	S5M3700X_0FA_STATUS11_INDEX
};

struct s5m3700x_irq_type_map irq_type_map[] = {
	/* addr_index,				irq,				type */
	{S5M3700X_001_IRQ1_INDEX,	ST_JO_R,			IRQ_ST_JACKOUT},
	{S5M3700X_001_IRQ1_INDEX,	ST_C_JI_R,			IRQ_ST_CMP_JACK_IN},
	{S5M3700X_001_IRQ1_INDEX,	ST_DEC_WTP_R,		IRQ_ST_WTJACK_DEC},
	{S5M3700X_001_IRQ1_INDEX,	ST_WT_JI_R,			IRQ_ST_WTJACK_IN},
	{S5M3700X_001_IRQ1_INDEX,	ST_WT_JO_R,			IRQ_ST_WTJACK_OUT},
	{S5M3700X_001_IRQ1_INDEX,	ST_IMP_CHK_DONE_R,	IRQ_ST_IMP_CHK},
	{S5M3700X_002_IRQ2_INDEX,	ST_DEC_POLE_R,		IRQ_ST_JACKDET},
	{S5M3700X_002_IRQ2_INDEX,	ST_ETC_R,			IRQ_ST_WTJACK_ETC},
	/* should check button release first */
	{S5M3700X_004_IRQ4_INDEX,	BTN_DET_F,			IRQ_ST_BTN_RELEASE},
	{S5M3700X_003_IRQ3_INDEX,	BTN_DET_R,			IRQ_ST_BTN_PRESS},
};

int s5m3700x_get_irq_type(struct s5m3700x_priv *s5m3700x, unsigned int event)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	int i = 0;
	int map_size = ARRAY_SIZE(irq_type_map);
	int ret = IRQ_ST_ERR;

	/* return irq for CCIC Event */
	if (event == JACK_HANDLER_EVENT_JACKIN)
		return IRQ_ST_JACKIN;
	else if (event == JACK_HANDLER_EVENT_JACKOUT)
		return IRQ_ST_JACKOUT;

	/* Normal Interrupt from Codec */
	for (i = 0; i < map_size; i++)
		if (jackdet->irq_val[irq_type_map[i].addr_index] & irq_type_map[i].irq)
			return irq_type_map[i].type;

	return ret;
}

void jack_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

void jack_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

int jack_set_wake_lock(struct s5m3700x_jack *jackdet)
{
	struct wakeup_source *ws = NULL;

	ws = wakeup_source_register(NULL, "s5m3700x-jack");

	if (ws == NULL)
		goto err;

	jackdet->jack_wakeup = ws;

	return 0;
err:
	return -1;
}

void jack_unregister_wake_lock(struct s5m3700x_jack *jackdet)
{
	wakeup_source_unregister(jackdet->jack_wakeup);
}

void reset_mic4_boost(struct s5m3700x_priv *s5m3700x)
{
	/* Ear-mic BST Reset */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0AF_BST_CTR, EN_ARESETB_BST4_MASK, EN_ARESETB_BST4_MASK);
	s5m3700x_usleep(100);
	s5m3700x_update_bits(s5m3700x, S5M3700X_0AF_BST_CTR, EN_ARESETB_BST4_MASK, 0);
}

/* Jack Control */
static int ant_mdet1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;

	s5m3700x_read(s5m3700x, S5M3700X_2DD_JACK_OTP0D, &value);

	ucontrol->value.integer.value[0] = (value & INP_SEL_R_MASK) >> INP_SEL_R_SHIFT;

	return 0;
}

static int ant_mdet1_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_2DD_JACK_OTP0D, INP_SEL_R_MASK, value << INP_SEL_R_SHIFT);

	return 0;
}

static int ant_mdet2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;

	s5m3700x_read(s5m3700x, S5M3700X_2DD_JACK_OTP0D, &value);

	ucontrol->value.integer.value[0] = (value & INP_SEL_L_MASK) >> INP_SEL_L_SHIFT;

	return 0;
}

static int ant_mdet2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_2DD_JACK_OTP0D, INP_SEL_L_MASK, value << INP_SEL_L_SHIFT);

	return 0;
}

static const struct snd_kcontrol_new s5m3700x_snd_jack_controls[] = {
	SOC_SINGLE("Jack DBNC In", S5M3700X_0D8_DCTR_DBNC1,
			JACK_DBNC_IN_SHIFT, JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack DBNC Out", S5M3700X_0D8_DCTR_DBNC1,
			JACK_DBNC_OUT_SHIFT, JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack DBNC IRQ In", S5M3700X_0D9_DCTR_DBNC2,
			JACK_DBNC_INT_IN_SHIFT, JACK_DBNC_INT_MAX, 0),

	SOC_SINGLE("Jack DBNC IRQ Out", S5M3700X_0D9_DCTR_DBNC2,
			JACK_DBNC_INT_OUT_SHIFT, JACK_DBNC_INT_MAX, 0),

	SOC_SINGLE("MDET Threshold", S5M3700X_0C4_ACTR_JD5,
			CTRV_ANT_MDET_SHIFT, 7, 0),

	SOC_SINGLE("MDET DBNC In", S5M3700X_0DB_DCTR_DBNC4,
			ANT_MDET_DBNC_IN_SHIFT, 15, 0),

	SOC_SINGLE("MDET DBNC Out", S5M3700X_0DB_DCTR_DBNC4,
			ANT_MDET_DBNC_OUT_SHIFT, 15, 0),

	SOC_SINGLE("MDET2 DBNC In", S5M3700X_0DC_DCTR_DBNC5,
			ANT_MDET_DBNC_IN_SHIFT, 15, 0),

	SOC_SINGLE("MDET2 DBNC Out", S5M3700X_0DC_DCTR_DBNC5,
			ANT_MDET_DBNC_OUT_SHIFT, 15, 0),

	SOC_SINGLE("Jack BTN DBNC", S5M3700X_0DD_DCTR_DBNC6,
			CTMD_BTN_DBNC_SHIFT, 15, 0),

	SOC_SINGLE_EXT("ANT MDET1 TRIM", SND_SOC_NOPM, 0, 7, 0,
			ant_mdet1_get, ant_mdet1_put),

	SOC_SINGLE_EXT("ANT MDET2 TRIM", SND_SOC_NOPM, 0, 7, 0,
			ant_mdet2_get, ant_mdet2_put),
};

void s5m3700x_register_jack_controls(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x)
{
	int ret = 0;

	dev_info(s5m3700x->dev, "%s enter\n", __func__);

	ret = snd_soc_add_component_controls(codec, s5m3700x_snd_jack_controls,
			ARRAY_SIZE(s5m3700x_snd_jack_controls));
	if (ret != 0)
		dev_err(s5m3700x->dev, "Failed to add Jack controls: %d\n", ret);
}
/*
 * s5m3700x_jack_variable_initialize()
 *
 * @s5m3700x: codec information struct
 *
 * Desc: Initialize jack_det struct variable as 0.
 */

void s5m3700x_jack_variable_initialize(struct s5m3700x_priv *s5m3700x, struct s5m3700x_jack *jackdet)
{
	/* initialize struct s5m3700x_jack */
	memset(jackdet, 0, sizeof(struct s5m3700x_jack));

	/* allocate pointer variable */
	s5m3700x->p_jackdet = jackdet;
	jackdet->codec = s5m3700x->codec;
	jackdet->p_s5m3700x = s5m3700x;
	jackdet->is_probe_done = false;

	/* Initialize mutex lock */
	mutex_init(&jackdet->key_lock);
	mutex_init(&jackdet->adc_lock);
	mutex_init(&jackdet->jack_det_lock);

	if (jack_set_wake_lock(jackdet) < 0) {
		pr_err("%s: jack_set_wake_lock fail\n", __func__);
		jack_unregister_wake_lock(jackdet);
	}

	jackdet->btn_state = BUTTON_RELEASE;
}

/*
 * s5m3700x_common_parse_dt() - Parsing device tree options about jack
 *
 * @s5m3700x: codec information struct
 *
 * Desc: Initialize jack_det struct variable as parsing device tree options.
 * Customer can tune this options. If not set by customer,
 * it use default value below defined.
 */
void s5m3700x_common_parse_dt(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	struct of_phandle_args args;
	int ret = 0, i = 0, temp = 0;
	unsigned int bias_v_conf;

	/* parsing IRQ */
	jackdet->irqb_gpio = of_get_named_gpio(dev->of_node, "s5m3700x-codec-int", 0);
	if (jackdet->irqb_gpio < 0)
		dev_err(dev, "%s cannot find irqb gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: irqb gpio = %d\n",
				__func__, jackdet->irqb_gpio);

	/*
	 * Set mic bias 1/2/3 voltages
	 */
	/* Mic Bias 1 */
	ret = of_property_read_u32(dev->of_node, "mic-bias1-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_VO_1_5V) &&
			 (bias_v_conf <= MICBIAS_VO_2_8V))) {
		jackdet->mic_bias1_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias1_voltage = MICBIAS_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias1-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	/* Mic Bias 2 */
	ret = of_property_read_u32(dev->of_node, "mic-bias2-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_VO_1_5V) &&
			 (bias_v_conf <= MICBIAS_VO_2_8V))) {
		jackdet->mic_bias2_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias2_voltage = MICBIAS_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias2-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	/* Mic Bias 3 */
	ret = of_property_read_u32(dev->of_node, "mic-bias3-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_VO_1_5V) &&
			 (bias_v_conf <= MICBIAS_VO_2_8V))) {
		jackdet->mic_bias3_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias3_voltage = MICBIAS_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias3-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	dev_info(dev, "Bias voltage values: bias1=%d, bias2=%d, bias3=%d\n",
			jackdet->mic_bias1_voltage,
			jackdet->mic_bias2_voltage,
			jackdet->mic_bias3_voltage);

	/* Mic det adc range */
	ret = of_parse_phandle_with_args(dev->of_node, "mic-adc-range", "#list-imp-cells", 0, &args);
	if (!ret) {
		for (i = 0; i < 2; i++)
			jackdet->mic_adc_range[i] = args.args[i];
	} else {
		jackdet->mic_adc_range[0] = S5M3700X_MIC_ADC_RANGE_0;
		jackdet->mic_adc_range[1] = S5M3700X_MIC_ADC_RANGE_1;
	}

	/* Jack Imp adc range */
	ret = of_parse_phandle_with_args(dev->of_node, "jack-imp-range", "#list-imp-cells", 0, &args);
	if (!ret) {
		for (i = 0; i < 5; i++)
			jackdet->jack_imp_range[i] = args.args[i];
	} else {
		jackdet->jack_imp_range[0] = S5M3700X_JACK_IMP_RANGE_0;
		jackdet->jack_imp_range[1] = S5M3700X_JACK_IMP_RANGE_1;
		jackdet->jack_imp_range[2] = S5M3700X_JACK_IMP_RANGE_2;
		jackdet->jack_imp_range[3] = S5M3700X_JACK_IMP_RANGE_3;
		jackdet->jack_imp_range[4] = S5M3700X_JACK_IMP_RANGE_4;
	}

	dev_info(dev, "jack imp range: %d %d %d %d %d.\n",
			jackdet->jack_imp_range[0],
			jackdet->jack_imp_range[1],
			jackdet->jack_imp_range[2],
			jackdet->jack_imp_range[3],
			jackdet->jack_imp_range[4]);

	/* OMTP det adc range */
	ret = of_property_read_u32(dev->of_node, "omtp-adc-range", &temp);
	if (!ret)
		jackdet->omtp_range = temp;
	else
		jackdet->omtp_range = S5M3700X_OMTP_ADC_DEFAULT;

	/* Button press adc value, a maximum of 4 buttons are supported */
	for (i = 0; i < 4; i++) {
		if (of_parse_phandle_with_args(dev->of_node,
					"but-zones-list", "#list-but-cells", i, &args))
			break;
		jackdet->btn_zones[i].adc_low = args.args[0];
		jackdet->btn_zones[i].adc_high = args.args[1];
	}

	/* WTP LDET threshold value */
	ret = of_parse_phandle_with_args(dev->of_node, "wtp-ldet-range", "#list-ldet-cells", 0, &args);
	if (!ret) {
		for (i = 0; i < 3; i++)
			jackdet->wtp_ldet_thd[i] = args.args[i];
	} else {
		for (i = 0; i < 3; i++)
			jackdet->wtp_ldet_thd[i] = S5M3700X_WTP_THD_DEFAULT;
	}
}

/* Request thread IRQ */
int s5m3700x_request_threaded_irq(struct s5m3700x_priv *s5m3700x,
					irqreturn_t (*s5m3700x_irq_thread)(int, void *))
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	int ret = 0;

	if (jackdet->irqb_gpio > 0) {
		jackdet->codec_irq = gpio_to_irq(jackdet->irqb_gpio);
		dev_info(s5m3700x->dev, "%s: codec_irq = %d\n", __func__, jackdet->codec_irq);

		if (jackdet->codec_irq > 0) {
			ret = request_threaded_irq(jackdet->codec_irq,
					NULL, s5m3700x_irq_thread,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"s5m3700x-irq", jackdet);
			if (ret)
				dev_err(s5m3700x->dev, "%s Failed to Request IRQ. ret: %d\n", __func__, ret);
			else {
				dev_info(s5m3700x->dev, "%s Request IRQ. ret: %d\n", __func__, ret);

				ret = enable_irq_wake(jackdet->codec_irq);
				if (ret < 0) {
					dev_err(s5m3700x->dev, "%s Failed to Enable Wakeup Source(%d)\n", __func__, ret);
					disable_irq_wake(jackdet->codec_irq);
				}
			}
		} else
			dev_err(s5m3700x->dev, "%s Failed gpio_to_irq. ret: %d\n", __func__, jackdet->codec_irq);
	} else {
		dev_err(s5m3700x->dev, "%s Failed get irqb_gpio %d\n", __func__, jackdet->irqb_gpio);
	}

	return ret;
}

/*Register Jack Detection Notifier */
int s5m3700x_register_notifier(struct s5m3700x_priv *s5m3700x,
		int (*s5m3700x_notifier_handler)(struct notifier_block *, unsigned long, void *))
{
	int ret = 0;
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* Allocate Notifier handler */
	jackdet->s5m3700x_jack_det_nb.notifier_call = s5m3700x_notifier_handler;

	/* Register Notifier */
	ret = blocking_notifier_chain_register(&s5m3700x_notifier, &(jackdet->s5m3700x_jack_det_nb));
	if (ret < 0)
		dev_err(s5m3700x->dev, "%s Failed to register notifier. ret: %d\n", __func__, ret);

	return ret;
}

/*
 * s5m3700x_register_jack
 */
int s5m3700x_register_jack(struct snd_soc_component *codec,
		struct s5m3700x_priv *s5m3700x, int type)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	int ret = 0;

	if (jackdet->headset_jack.jack == NULL) {
		ret = snd_soc_card_jack_new(codec->card,
					    "S5M3700X Headset Input", type,
					    &jackdet->headset_jack, NULL, 0);
		if (ret) {
			dev_info(s5m3700x->dev, "%s: failed to create new jack %d\n", __func__, ret);
			return ret;
		}
		dev_info(s5m3700x->dev, "%s: jack is registered\n", __func__);
	} else {
		dev_info(s5m3700x->dev, "%s: jack is already registered\n", __func__);
	}
	return ret;
}

int s5m3700x_register_button(struct snd_soc_component *codec,
		struct s5m3700x_priv *s5m3700x, int type)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	int ret = 0;

	if (jackdet->headset_button.jack == NULL) {
		ret = snd_soc_card_jack_new(codec->card,
					    "S5M3700X Headset Button", type,
					    &jackdet->headset_button, NULL, 0);
		if (ret) {
			dev_info(s5m3700x->dev, "%s: failed to create new button jack %d\n", __func__, ret);
			return ret;
		}
		/* keycode */
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_0, KEY_MEDIA); //226
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_1, KEY_VOICECOMMAND); //0x246
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_2, KEY_VOLUMEUP); //115
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN); //114
		dev_info(s5m3700x->dev, "%s: button jack is registered\n", __func__);
	} else {
		dev_info(s5m3700x->dev, "%s: button jack is already registered\n", __func__);
	}
	return ret;
}

/*
 * report jack type
 */
void s5m3700x_report_jack_type(struct s5m3700x_priv *s5m3700x, int type, int insert)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	if (insert) {
		switch (jackdet->cur_jack_state) {
		case JACK_3POLE:
		case USB_3POLE:
			snd_soc_jack_report(&jackdet->headset_jack, SND_JACK_HEADPHONE, type);
			break;
		case JACK_4POLE:
		case USB_4POLE:
			snd_soc_jack_report(&jackdet->headset_jack, SND_JACK_HEADSET, type);
			break;
		case JACK_AUX:
			snd_soc_jack_report(&jackdet->headset_jack, SND_JACK_LINEOUT, type);
			break;
		default:
			dev_info(s5m3700x->dev, "%s Unsupported jack state\n", __func__);
			break;
		}
	} else {
		snd_soc_jack_report(&jackdet->headset_jack, 0, type);
	}
}

/*
 * report button press / release
 */
static int s5m3700x_get_btn_mask(struct s5m3700x_priv *s5m3700x, int btn_adc)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct jack_btn_zone *btn_zones = jackdet->btn_zones;
	int num_btn_zones = ARRAY_SIZE(jackdet->btn_zones), i = 0, mask = SND_JACK_BTN_0;

	for (i = 0; i < num_btn_zones; i++) {
		if ((btn_adc >= btn_zones[i].adc_low)
			&& (btn_adc <= btn_zones[i].adc_high))
			break;
	}

	switch (i) {
	case 0:
		mask = SND_JACK_BTN_0;
		break;
	case 1:
		mask = SND_JACK_BTN_1;
		break;
	case 2:
		mask = SND_JACK_BTN_2;
		break;
	case 3:
		mask = SND_JACK_BTN_3;
		break;
	default:
		dev_err(s5m3700x->dev, "%s : Unsupported button press (adc value %d)\n", __func__, btn_adc);
		mask = -1;
		break;
	}
	return mask;
}

void s5m3700x_report_button_type(struct s5m3700x_priv *s5m3700x,
		int type, int state, int btn_adc)
{
	int btn_mask = 0;
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	if (state == BUTTON_RELEASE) {
		/* Mic unmute for remove click noise */
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, false);

		snd_soc_jack_report(&jackdet->headset_button, 0, type);
		jackdet->btn_state = BUTTON_RELEASE;
		dev_info(s5m3700x->dev, "%s : Release Button\n", __func__);
	} else {
		/* Mic mute for remove click noise */
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);

		btn_mask = s5m3700x_get_btn_mask(s5m3700x, btn_adc);
		if(btn_mask > 0) {
			snd_soc_jack_report(&jackdet->headset_button, btn_mask, type);
			jackdet->btn_state = BUTTON_PRESS;
			dev_info(s5m3700x->dev, "%s : Press Button (0x%x)\n", __func__, btn_mask);
		}
	}
}

bool s5m3700x_button_error_check(struct s5m3700x_priv *s5m3700x, int btn_irq_state)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* Terminate workqueue Cond1: jack is not detected */
	if (jackdet->cur_jack_state & (JACK_OUT|USB_OUT)) {
		dev_err(s5m3700x->dev, "Skip button events because jack is not detected.\n");
		if (jackdet->btn_state == BUTTON_PRESS) {
			jackdet->btn_state = BUTTON_RELEASE;
			s5m3700x_report_button_type(s5m3700x, S5M3700X_BUTTON_MASK, BUTTON_RELEASE, 0);
			dev_err(s5m3700x->dev, "force report BUTTON_RELEASE because button state is pressed.\n");
		}
		return false;
	}

	/* Terminate workqueue Cond2: 3 pole earjack */
	if (!(jackdet->cur_jack_state & (JACK_4POLE|USB_4POLE))) {
		dev_err(s5m3700x->dev, "Skip button events for 3 pole earjack.\n");
		return false;
	}

	/* Terminate workqueue Cond3: state is not changed */
	if (jackdet->btn_state == btn_irq_state) {
		dev_err(s5m3700x->dev, "button state is not changed.\n");
		return false;
	}

	return true;
}

/* call registered notifier handler */
void s5m3700x_call_notifier(struct s5m3700x_priv *s5m3700x, int insert)
{
	blocking_notifier_call_chain(&s5m3700x_notifier, insert, s5m3700x);
}

void s5m3700x_read_irq(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	unsigned int temp = 0, i = 0;

	for (i = 0; i < S5M3700X_NUM_JACK_IRQ; i++) {
		s5m3700x_read_only_hardware(s5m3700x, s5m3700x_jack_irq_addr[i], &temp);
		jackdet->irq_val[i] = (u8) temp;
	}

	dev_info(s5m3700x->dev,
			"[IRQ] %s(%d) 0x1:%02x 0x2:%02x 0x3:%02x 0x4:%02x 0x5:%02x 0x6:%02x st1:%02x st2:%02x st3:%02x st4:%02x\n",
			__func__, __LINE__, jackdet->irq_val[0], jackdet->irq_val[1],
			jackdet->irq_val[2], jackdet->irq_val[3], jackdet->irq_val[4],
			jackdet->irq_val[5], jackdet->irq_val[6], jackdet->irq_val[7],
			jackdet->irq_val[8], jackdet->irq_val[9]);
}

void s5m3700x_hp_trim_initialize(struct s5m3700x_priv *s5m3700x)
{
	unsigned int i = 0, temp[HP_TRIM_CNT];

	dev_info(s5m3700x->dev, "%s called, setting hp trim values.\n", __func__);

	for (i = 0; i < HP_TRIM_CNT; i++) {
		/* Reading HP Normal Trim */
		s5m3700x_read(s5m3700x, S5M3700X_200_HP_OFFSET_L0 + i, &temp[i]);
		s5m3700x->hp_normal_trim[i] = temp[i];

		/* Setting HP UHQA Trim */
		s5m3700x->hp_uhqa_trim[i] = ((s5m3700x->hp_normal_trim[i] * 777) + 500) / 1000;
	}
}

void s5m3700x_wtp_threshold_initialize(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	unsigned int v1, v2;
	int index = S5M3700X_WTP_THD_LOW_RANGE;

	/* LOWR_WTP_LDET */
	index = S5M3700X_WTP_THD_LOW_RANGE;
	v1 = 0;
	v2 = 0;
	if(jackdet->wtp_ldet_thd[index] == S5M3700X_WTP_THD_DEFAULT)
	{
		s5m3700x_read(s5m3700x, S5M3700X_2DA_JACK_OTP0A, &v1);
		s5m3700x_read(s5m3700x, S5M3700X_2DC_JACK_OTP0C, &v2);

		v2 = v2 & 0xF0;
		jackdet->wtp_ldet_thd[index] = (v2 << 4) + v1;
	} else {
		v1 = jackdet->wtp_ldet_thd[index] & 0xFF;
		v2 = (jackdet->wtp_ldet_thd[index] >> 8 << 4) & 0xF0;

		s5m3700x_write(s5m3700x, S5M3700X_2DA_JACK_OTP0A, v1);
		s5m3700x_update_bits(s5m3700x, S5M3700X_2DC_JACK_OTP0C, 0xF0, v2);
	}

	/* MIDR_WTP_LDET */
	index = S5M3700X_WTP_THD_MID_RANGE;
	v1 = 0;
	v2 = 0;
	if(jackdet->wtp_ldet_thd[index] == S5M3700X_WTP_THD_DEFAULT)
	{
		s5m3700x_read(s5m3700x, S5M3700X_2DE_JACK_OTP0E, &v1);
		s5m3700x_read(s5m3700x, S5M3700X_2DF_JACK_OTP0F, &v2);

		v1 = v1 & 0xF0;
		jackdet->wtp_ldet_thd[index] = (v1 << 4) + v2;
	} else {
		v1 = (jackdet->wtp_ldet_thd[index] >> 8 << 4) & 0xF0;
		v2 = jackdet->wtp_ldet_thd[index] & 0xFF;

		s5m3700x_update_bits(s5m3700x, S5M3700X_2DE_JACK_OTP0E,	0xF0, v1);
		s5m3700x_write(s5m3700x, S5M3700X_2DF_JACK_OTP0F, v2);
	}

	/* HIGHHR_WTP_LDET */
	index = S5M3700X_WTP_THD_HIGH_RANGE;
	v1 = 0;
	v2 = 0;
	if(jackdet->wtp_ldet_thd[index] == S5M3700X_WTP_THD_DEFAULT)
	{
		s5m3700x_read(s5m3700x, S5M3700X_2DB_JACK_OTP0B, &v1);
		s5m3700x_read(s5m3700x, S5M3700X_2DC_JACK_OTP0C, &v2);

		v2 = v2 & 0x0F;
		jackdet->wtp_ldet_thd[index] = (v2 << 8) + v1;
	} else {
		v1 = jackdet->wtp_ldet_thd[index] & 0xFF;
		v2 = (jackdet->wtp_ldet_thd[index] >> 8) & 0x0F;

		s5m3700x_write(s5m3700x, S5M3700X_2DB_JACK_OTP0B, v1);
		s5m3700x_update_bits(s5m3700x, S5M3700X_2DC_JACK_OTP0C,	0x0F, v2);
	}

	dev_info(dev, "%s : wtp ldet range %d %d %d\n", __func__
					,jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_LOW_RANGE]
					,jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_MID_RANGE]
					,jackdet->wtp_ldet_thd[S5M3700X_WTP_THD_HIGH_RANGE]);
}

/*
 * s5m3700x_configure_mic_bias() - Configure mic bias voltage
 *
 * @jackdet: jack information struct
 *
 * Desc: Configure the mic1 and mic2 bias voltages with default value
 * or the value received from the device tree.
 * Also configure the internal LDO voltage.
 */
void s5m3700x_configure_mic_bias(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	/* Configure Mic1 Bias Voltage */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C8_ACTR_MCB4,
			CTRV_MCB1_MASK, jackdet->mic_bias1_voltage << CTRV_MCB1_SHIFT);

	/* Configure Mic2 Bias Voltage */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C9_ACTR_MCB5,
			CTRV_MCB2_MASK, jackdet->mic_bias2_voltage << CTRV_MCB2_SHIFT);

	/* Configure Mic3 Bias Voltage */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C9_ACTR_MCB5,
			CTRV_MCB3_MASK, jackdet->mic_bias3_voltage << CTRV_MCB3_SHIFT);
}

void s5m3700x_read_imp_otp(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	bool msb;
	unsigned int jack_otp[4];

	/* Jack OTP Setting */
	s5m3700x_read(s5m3700x, S5M3700X_2D2_JACK_OTP02, &jack_otp[0]);
	s5m3700x_read(s5m3700x, S5M3700X_2D3_JACK_OTP03, &jack_otp[1]);
	s5m3700x_read(s5m3700x, S5M3700X_2D4_JACK_OTP04, &jack_otp[2]);
	s5m3700x_read(s5m3700x, S5M3700X_2D5_JACK_OTP05, &jack_otp[3]);

	/* IMP_CAL_P1 */
	jackdet->imp_p[0] = jack_otp[1];
	/* IMP_CAL_P2 */
	jackdet->imp_p[1] = ((jack_otp[0] & 0x70) << 4) + jack_otp[2];
	/* IMP_CAL_P3 */
	jackdet->imp_p[2] = ((jack_otp[0] & 0x0C) << 6) + jack_otp[3];
	msb = jackdet->imp_p[2] & 0x200;
	if (msb)
		jackdet->imp_p[2] = -(0x400 - jackdet->imp_p[2]);

	dev_info(s5m3700x->dev, "%s called. p1: %d, p2: %d, p3: %d\n",
			__func__, jackdet->imp_p[0],
			jackdet->imp_p[1], jackdet->imp_p[2]);
}

void s5m3700x_jack_register_exit(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	/* IRQ Masking */
	s5m3700x_write(s5m3700x, S5M3700X_008_IRQ1M, 0xFF);
	s5m3700x_write(s5m3700x, S5M3700X_009_IRQ2M, 0xFF);
	s5m3700x_write(s5m3700x, S5M3700X_00A_IRQ3M, 0xFF);
	s5m3700x_write(s5m3700x, S5M3700X_00B_IRQ4M, 0xFF);
}

bool s5m3700x_check_jack_state_sequence(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	bool ret = false;
	unsigned int cur_jack, prv_jack;

	cur_jack = jackdet->cur_jack_state;
	prv_jack = jackdet->prv_jack_state;

	switch (cur_jack) {
	case JACK_OUT:
		if (prv_jack & (JACK_IN | JACK_WTP_ST | JACK_POLE_DEC | JACK_IMP | JACK_CMP))
			ret = true;
		break;
	case JACK_CMP:
		if (prv_jack & (JACK_OUT | JACK_WTP_CJI | JACK_WTP_ETC))
			ret = true;
		break;
	case JACK_WTP_DEC:
		if (prv_jack & (JACK_OUT | JACK_CMP | JACK_WTP_JIO))
			ret = true;
		break;
	case JACK_WTP_CJI:
		if (prv_jack & JACK_WTP_DEC)
			ret = true;
		break;
	case JACK_WTP_ETC:
		if (prv_jack & JACK_WTP_DEC)
			ret = true;
		break;
	case JACK_WTP_POLL:
		if (prv_jack & JACK_WTP_DEC)
			ret = true;
		break;
	case JACK_WTP_JO:
		if (prv_jack & JACK_WTP_DEC)
			ret = true;
		break;
	case JACK_WTP_JIO:
		if (prv_jack & JACK_WTP_POLL)
			ret = true;
		break;
	case JACK_IMP:
		if (prv_jack & JACK_CMP)
			ret = true;
		break;
	case JACK_POLE_DEC:
		if (prv_jack & (JACK_OUT | JACK_IMP))
			ret = true;
		break;
	case JACK_4POLE:
		if (prv_jack & JACK_POLE_DEC)
			ret = true;
		break;
	case JACK_3POLE:
		if (prv_jack & JACK_POLE_DEC)
			ret = true;
		break;
	case JACK_AUX:
		if (prv_jack & (JACK_POLE_DEC | JACK_WTP_ETC))
			ret = true;
		break;
	case JACK_OMTP:
		if (prv_jack & JACK_POLE_DEC)
			ret = true;
		break;
	case USB_OUT:
		if (prv_jack & (USB_IN | USB_INSERTING))
			ret = true;
		break;
	case USB_INIT:
		if (prv_jack & USB_OUT)
			ret = true;
		break;
	case USB_JACK_CMP:
		if (prv_jack & (USB_INIT | USB_IN))
			ret = true;
		break;
	case USB_MCHK_DONE:
		if (prv_jack & USB_JACK_CMP)
			ret = true;
		break;
	case USB_POLE_DEC:
		if (prv_jack & USB_MCHK_DONE)
			ret = true;
		break;
	case USB_3POLE:
		if (prv_jack & USB_POLE_DEC)
			ret = true;
		break;
	case USB_4POLE:
		if (prv_jack & USB_POLE_DEC)
			ret = true;
		break;
	}

	if (!ret) {
		dev_err(s5m3700x->dev, "%s Jack state machine error! prv: %s, cur: %s\n", __func__,
				s5m3700x_return_status_name(prv_jack),
				s5m3700x_return_status_name(cur_jack));
	}
	return ret;
}

char *s5m3700x_return_status_name(unsigned int status)
{
	switch (status) {
	case JACK_OUT:
		return "JACK_OUT";
	case JACK_POLE_DEC:
		return "JACK_POLE_DEC";
	case JACK_3POLE:
		return "JACK_3POLE";
	case JACK_4POLE:
		return "JACK_4POLE";
	case JACK_AUX:
		return "JACK_AUX";
	case JACK_OMTP:
		return "JACK_OMTP";
	case JACK_CMP:
		return "JACK_CMP";
	case JACK_IMP:
		return "JACK_IMP";
	case JACK_WTP_DEC:
		return "JACK_WTP_DEC";
	case JACK_WTP_JIO:
		return "JACK_WTP_JIO";
	case JACK_WTP_CJI:
		return "JACK_WTP_CJI";
	case JACK_WTP_ETC:
		return "JACK_WTP_ETC";
	case JACK_WTP_POLL:
		return "JACK_WTP_POLL";
	case JACK_WTP_JO:
		return "JACK_WTP_JO";
	case JACK_IN:
		return "JACK_IN";
	case JACK_WTP_ST:
		return "JACK_WTP_ST";
	}

	switch (status) {
	case USB_OUT:
		return "USB_OUT";
	case USB_INIT:
		return "USB_INIT";
	case USB_JACK_CMP:
		return "USB_JACK_CMP";
	case USB_JACK_IMP:
		return "USB_JACK_IMP";
	case USB_POLE_DEC:
		return "USB_POLE_DEC";
	case USB_3POLE:
		return "USB_3POLE";
	case USB_4POLE:
		return "USB_4POLE";
	case USB_AUX:
		return "USB_AUX";
	case USB_MCHK_DONE:
		return "USB_MCHK_DONE";
	case USB_IN:
		return "USB_IN";
	case USB_INSERTING:
		return "USB_INSERTING";
	}
	return "No status";
}

char *s5m3700x_print_jack_type_state(int cur_jack_state)
{

	if (cur_jack_state & JACK_IN) {
		if (cur_jack_state & JACK_4POLE)
			return "4POLE";
		else if (cur_jack_state & JACK_AUX)
			return "AUX";
		else
			return "3POLE";
	} else if (cur_jack_state & USB_IN) {
		if (cur_jack_state & USB_4POLE)
			return "4POLE";
		else
			return "3POLE";
	}

	return "";
}

void s5m3700x_print_status_change(struct device *dev, unsigned int prv_jack, unsigned int cur_jack)
{
	dev_info(dev, "Priv: %s -> Cur: %s\n"
		, s5m3700x_return_status_name(prv_jack), s5m3700x_return_status_name(cur_jack));
}
