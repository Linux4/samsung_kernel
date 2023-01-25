/* Copyright (c) 2011-2013 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/input.h>
#include "msm-pcm-routing.h"
#include "../codecs/wcd9310.h"
#include <mach/msm8960-gpio.h>
#if defined(CONFIG_MACH_AEGIS2)
#include <linux/i2c/fsa9485.h>
#endif

#ifndef GPIO_MAIN_MIC_BIAS
#define GPIO_MAIN_MIC_BIAS -1
#endif

#if defined(CONFIG_MACH_AEGIS2)
#define SW_AUDIO		((2 << 5) | (2 << 2) | (1 << 1) | (1 << 0))
#define SW_USB_OPEN		(1 << 0)
#endif
extern unsigned int system_rev;

/* 8960 machine driver */

#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_IRQ_BASE (NR_MSM_IRQS + NR_GPIO_IRQS)
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)

#define MSM8960_SPK_ON 1
#define MSM8960_SPK_OFF 0

#define msm8960_SLIM_0_RX_MAX_CHANNELS		2
#define msm8960_SLIM_0_TX_MAX_CHANNELS		4

#define SAMPLE_RATE_8KHZ 8000
#define SAMPLE_RATE_16KHZ 16000

#define BOTTOM_SPK_AMP_POS	0x1
#define BOTTOM_SPK_AMP_NEG	0x2
#define TOP_SPK_AMP_POS		0x4
#define TOP_SPK_AMP_NEG		0x8
#define TOP_SPK_AMP		0x10

#define GPIO_AUX_PCM_DOUT 63
#define GPIO_AUX_PCM_DIN 64
#define GPIO_AUX_PCM_SYNC 65
#define GPIO_AUX_PCM_CLK 66
#define GPIO_SPKR_I2S_MCLK     59
#define GPIO_SPKR_I2S_RX_SCK   60
#define GPIO_SPKR_I2S_RX_DOUT  61
#define GPIO_SPKR_I2S_RX_WS    62

#define GPIO_SPKR_I2S_TX_SCK   55
#define GPIO_SPKR_I2S_TX_WS            56
#define GPIO_SPKR_I2S_TX_D0            57
#define NO_OF_BITS_PER_SAMPLE  16

#define I2S_MCLK_RATE	12288000
#define I2S_MIC_MCLK_RATE 1536000

#define TABLA_EXT_CLK_RATE 12288000

#define TABLA_MBHC_DEF_BUTTONS 8
#define TABLA_MBHC_DEF_RLOADS 5

#define NUM_EXT_SPK_AMP_STATES 2

#define JACK_DETECT_GPIO 38
#define JACK_DETECT_INT PM8921_GPIO_IRQ(PM8921_IRQ_BASE, JACK_DETECT_GPIO)
#define JACK_US_EURO_SEL_GPIO 35
#define GPIO_DETECT_USED false

static u32 top_spk_pamp_gpio  = PM8921_GPIO_PM_TO_SYS(18);
static u32 bottom_spk_pamp_gpio = PM8921_GPIO_PM_TO_SYS(19);
static int msm8960_spk_control;
static int msm8960_ext_bottom_spk_pamp;
static int msm8960_ext_top_spk_pamp;
static int msm8960_slim_0_rx_ch = 1;
static int msm8960_slim_0_tx_ch = 1;

static int msm8960_btsco_rate = SAMPLE_RATE_8KHZ;
static int msm8960_btsco_ch = 1;
static int hdmi_rate_variable;
static int msm_hdmi_rx_ch = 2;
static int msm8960_auxpcm_rate = SAMPLE_RATE_8KHZ;

static struct clk *codec_clk;
static int clk_users;

static int msm8960_audio_gpios_configured;

static struct snd_soc_jack hs_jack;
static struct snd_soc_jack button_jack;
static atomic_t auxpcm_rsc_ref;

static struct snd_soc_jack volumedown_jack;
static struct snd_soc_jack volumeup_jack;

static bool hs_detect_extn_cable;
module_param(hs_detect_extn_cable, bool, 0444);
MODULE_PARM_DESC(hs_detect_extn_cable, "Enable extension cable feature");


static int msm8960_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);
/*
static bool msm8960_swap_gnd_mic(struct snd_soc_codec *codec);
*/
static struct tabla_mbhc_config mbhc_cfg = {
	.headset_jack = &hs_jack,
	.button_jack = &button_jack,
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = TABLA_MICBIAS2,
	.mclk_cb_fn = msm8960_enable_codec_ext_clk,
	.mclk_rate = TABLA_EXT_CLK_RATE,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 1,
	.swap_gnd_mic = NULL,
	.detect_extn_cable = false,
};

static u32 us_euro_sel_gpio = PM8921_GPIO_PM_TO_SYS(JACK_US_EURO_SEL_GPIO);

static int msm8960_i2s_rx_ch = 1;
static int msm8960_i2s_tx_ch = 1;
static int msm8960_i2s_spk_control;
/* static struct clk *rx_osr_clk; */
static struct clk *rx_bit_clk;
static struct clk *tx_osr_clk;
static struct clk *tx_bit_clk;

struct ext_amp_work {
	struct delayed_work dwork;
};

static struct ext_amp_work ext_amp_dwork;
#if defined(CONFIG_MACH_AEGIS2)
static struct ext_amp_work bottom_amp_dwork;
#endif

/* Work queue for delaying the amp power on-off to
remove the static noise during SPK_PA enable */
static void external_speaker_amp_work(struct work_struct *work)
{
	pr_debug("%s :: Top Speaker Amp enable\n", __func__);
	gpio_direction_output(top_spk_pamp_gpio, 1);
	pr_debug("%s: slepping 4 ms after turning on external "
			" Top Speaker Ampl\n", __func__);
	usleep_range(4000, 4000);
}

#if defined(CONFIG_MACH_AEGIS2)
static void bottom_speaker_amp_work(struct work_struct *work)
{
	pr_debug("%s :: bottom Speaker Amp enable\n", __func__);
	fsa9485_checkandhookaudiodockfornoise(SW_AUDIO);
}
#endif

static void msm8960_ext_spk_power_amp_on(u32 spk)
{
	if (spk & (BOTTOM_SPK_AMP_POS | BOTTOM_SPK_AMP_NEG)) {

		if ((msm8960_ext_bottom_spk_pamp & BOTTOM_SPK_AMP_POS) &&
			(msm8960_ext_bottom_spk_pamp & BOTTOM_SPK_AMP_NEG)) {

			pr_debug("%s() External Bottom Speaker Ampl already "
				"turned on. spk = 0x%08x\n", __func__, spk);
			return;
		}

		msm8960_ext_bottom_spk_pamp |= spk;

		if ((msm8960_ext_bottom_spk_pamp & BOTTOM_SPK_AMP_POS) &&
			(msm8960_ext_bottom_spk_pamp & BOTTOM_SPK_AMP_NEG)) {

			pr_debug("%s Enabling Bottom Speaker\n", __func__);
			gpio_direction_output(bottom_spk_pamp_gpio, 1);
			pr_debug("%s: slepping 4 ms after turning on external "
				" Bottom Speaker Ampl\n", __func__);
			usleep_range(4000, 4000);
#if defined(CONFIG_MACH_AEGIS2)
			schedule_delayed_work(
			&bottom_amp_dwork.dwork,
			msecs_to_jiffies(50));
#endif
		}

	} else if (spk & (TOP_SPK_AMP_POS | TOP_SPK_AMP_NEG)) {

		if ((msm8960_ext_top_spk_pamp & TOP_SPK_AMP_POS) &&
			(msm8960_ext_top_spk_pamp & TOP_SPK_AMP_NEG)) {

			pr_debug("%s() External Top Speaker Ampl already"
				"turned on. spk = 0x%08x\n", __func__, spk);
			return;
		}

		msm8960_ext_top_spk_pamp |= spk;

		if ((msm8960_ext_top_spk_pamp & TOP_SPK_AMP_POS) &&
			(msm8960_ext_top_spk_pamp & TOP_SPK_AMP_NEG)) {
			/* Delaying the amp power_on to remove the static noise
			during SPK_PA enable */
			schedule_delayed_work(
			&ext_amp_dwork.dwork,
#ifdef CONFIG_MACH_M2_ATT
			msecs_to_jiffies(60));
#else
			msecs_to_jiffies(50));
#endif			
		}
	} else  {

		pr_err("%s: ERROR : Invalid External Speaker Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void msm8960_ext_spk_power_amp_off(u32 spk)
{
	if (spk & (BOTTOM_SPK_AMP_POS | BOTTOM_SPK_AMP_NEG)) {

		if (!msm8960_ext_bottom_spk_pamp)
			return;

		pr_debug("%s Disabling Bottom Speaker\n", __func__);
		gpio_direction_output(bottom_spk_pamp_gpio, 0);
		msm8960_ext_bottom_spk_pamp = 0;

		pr_debug("%s: sleeping 4 ms after turning off external Bottom"
			" Speaker Ampl\n", __func__);

		usleep_range(4000, 4000);
#if defined(CONFIG_MACH_AEGIS2)
		fsa9485_checkandhookaudiodockfornoise(SW_USB_OPEN);
#endif
	} else if (spk & (TOP_SPK_AMP_POS | TOP_SPK_AMP_NEG)) {

		if (!msm8960_ext_top_spk_pamp)
			return;
		gpio_direction_output(top_spk_pamp_gpio, 0);
		pr_debug("%s: slepping 4 ms after turning off external "
			" Top Speaker Ampl\n", __func__);
		msm8960_ext_top_spk_pamp = 0;
	} else  {

		pr_err("%s: ERROR : Invalid Ext Spk Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void msm8960_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	mutex_lock(&dapm->codec->mutex);

	pr_debug("%s: msm8960_spk_control = %d", __func__, msm8960_spk_control);
	if (msm8960_spk_control == MSM8960_SPK_ON ||
				msm8960_i2s_spk_control == MSM8960_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Neg");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Neg");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Bottom Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Bottom Neg");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Top Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Top Neg");
	}

	snd_soc_dapm_sync(dapm);
	mutex_unlock(&dapm->codec->mutex);
}

static int msm8960_get_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_spk_control = %d", __func__, msm8960_spk_control);
	ucontrol->value.integer.value[0] = msm8960_spk_control;
	return 0;
}
static int msm8960_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8960_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm8960_spk_control = ucontrol->value.integer.value[0];
	msm8960_ext_control(codec);
	return 1;
}
static int msm8960_spkramp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("%s() %x\n", __func__, SND_SOC_DAPM_EVENT_ON(event));

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strncmp(w->name, "Ext Spk Bottom Pos", 18))
			msm8960_ext_spk_power_amp_on(BOTTOM_SPK_AMP_POS);
		else if (!strncmp(w->name, "Ext Spk Bottom Neg", 18))
			msm8960_ext_spk_power_amp_on(BOTTOM_SPK_AMP_NEG);
		else if (!strncmp(w->name, "Ext Spk Top Pos", 15))
			msm8960_ext_spk_power_amp_on(TOP_SPK_AMP_POS);
		else if  (!strncmp(w->name, "Ext Spk Top Neg", 15))
			msm8960_ext_spk_power_amp_on(TOP_SPK_AMP_NEG);
		else if  (!strncmp(w->name, "Ext Spk Top", 12))
			msm8960_ext_spk_power_amp_on(TOP_SPK_AMP);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}

	} else {
		if (!strncmp(w->name, "Ext Spk Bottom Pos", 18))
			msm8960_ext_spk_power_amp_off(BOTTOM_SPK_AMP_POS);
		else if (!strncmp(w->name, "Ext Spk Bottom Neg", 18))
			msm8960_ext_spk_power_amp_off(BOTTOM_SPK_AMP_NEG);
		else if (!strncmp(w->name, "Ext Spk Top Pos", 15))
			msm8960_ext_spk_power_amp_off(TOP_SPK_AMP_POS);
		else if  (!strncmp(w->name, "Ext Spk Top Neg", 15))
			msm8960_ext_spk_power_amp_off(TOP_SPK_AMP_NEG);
		else if  (!strncmp(w->name, "Ext Spk Top", 12))
			msm8960_ext_spk_power_amp_off(TOP_SPK_AMP);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	}
	return 0;
}

static int msm8960_bias_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("GPIO BIAS UP!!!%d\n", SND_SOC_DAPM_EVENT_ON(event));
#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
	if (system_rev == BOARD_REV00)
		gpio_direction_output(GPIO_MAIN_MIC_BIAS_REV00,
				SND_SOC_DAPM_EVENT_ON(event));
	else if (system_rev == BOARD_REV01)
		gpio_direction_output(GPIO_MAIN_MIC_BIAS_REV01,
				SND_SOC_DAPM_EVENT_ON(event));
	else if (system_rev >= BOARD_REV02)
		gpio_direction_output(GPIO_MAIN_MIC_BIAS_REV02,
				SND_SOC_DAPM_EVENT_ON(event));
#else
	/* temporary code for old HW revision of JASPER */
	if (machine_is_JASPER() && system_rev < BOARD_REV04) {
		gpio_direction_output(18, SND_SOC_DAPM_EVENT_ON(event));
	} else {
		gpio_direction_output(GPIO_MAIN_MIC_BIAS,
		SND_SOC_DAPM_EVENT_ON(event));
	}
#endif
	return 0;
}

/* Not used */
/*
static int msm8960_cdc_cp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("%s %d\n", __func__, SND_SOC_DAPM_EVENT_ON(event));
	gpio_direction_output(PM8921_GPIO_PM_TO_SYS(17),
	SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}
*/

static int msm8960_cdc_vps_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s %d\n", __func__, SND_SOC_DAPM_EVENT_ON(event));
	gpio_direction_output(PM8921_GPIO_PM_TO_SYS(5),
	SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}

static struct mutex cdc_mclk_mutex;

static int msm8960_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm)
{
	int r = 0;
	pr_info("%s: enable = %d\n", __func__, enable);

	mutex_lock(&cdc_mclk_mutex);
	if (enable) {
		clk_users++;
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users == 1) {
			if (codec_clk) {
				clk_set_rate(codec_clk, TABLA_EXT_CLK_RATE);
				clk_prepare_enable(codec_clk);
				tabla_mclk_enable(codec, 1, dapm);
			} else {
				pr_err("%s: Error setting Tabla MCLK\n",
				       __func__);
				clk_users--;
				r = -EINVAL;
			}
		}
	} else {
		if (clk_users > 0) {
			clk_users--;
			pr_debug("%s: clk_users = %d\n", __func__, clk_users);
			if (clk_users == 0) {
				pr_debug("%s: disabling MCLK. clk_users = %d\n",
					 __func__, clk_users);
				tabla_mclk_enable(codec, 0, dapm);
				clk_disable_unprepare(codec_clk);
			}
		} else {
			pr_err("%s: Error releasing Tabla MCLK\n", __func__);
			r = -EINVAL;
		}
	}
	mutex_unlock(&cdc_mclk_mutex);
	return r;
}
/*
static bool msm8960_swap_gnd_mic(struct snd_soc_codec *codec)
{
	int value = gpio_get_value_cansleep(us_euro_sel_gpio);
	pr_debug("%s: US EURO select switch %d to %d\n", __func__, value,
		 !value);
	gpio_set_value_cansleep(us_euro_sel_gpio, !value);
	return true;
}
*/
static int msm8960_mclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm8960_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm8960_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm8960_dapm_widgets_org[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8960_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Ext Spk Bottom Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_SPK("Ext Spk Top Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Top Neg", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Top", msm8960_spkramp_event),

	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),

	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),

};

static const struct snd_soc_dapm_route common_audio_map_org[] = {
	{"EAR_RX_BIAS", NULL, "MCLK"},
	{"RX_BIAS", NULL, "MCLK"},
	{"LDO_H", NULL, "MCLK"},

	/* Speaker path */
	{"Ext Spk Bottom Pos", NULL, "LINEOUT1"},
	{"Ext Spk Bottom Neg", NULL, "LINEOUT5"},

	{"Ext Spk Top Pos", NULL, "LINEOUT3"},
	{"Ext Spk Top Neg", NULL, "LINEOUT4"},
	{"Ext Spk Top", NULL, "LINEOUT5"},

	/* Microphone path */
	/**
	 *Samsung uses AMIC4 for Handset sub Mic
	 *AMIC4 uses external MIC BIAS1
	 */
	{"AMIC3", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Handset Mic"},

	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},

	{"AMIC4", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Sub Mic"},

	/**
	 * AMIC3 and AMIC4 inputs are connected to ANC microphones
	 * These mics are biased differently on CDP and FLUID
	 * routing entries below are based on bias arrangement
	 * on FLUID.
	 */
	{ "AMIC3", NULL, "MIC BIAS1 External" },

	{"HEADPHONE", NULL, "LDO_H"},

	/**
	 * The digital Mic routes are setup considering
	 * fluid as default device.
	 */

	/**
	 * Digital Mic1. Front Bottom left Digital Mic on Fluid and MTP.
	 * Digital Mic GM5 on CDP mainboard.
	 * Conncted to DMIC2 Input on Tabla codec.
	 */
	{"DMIC2", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic1"},

	/**
	 * Digital Mic2. Front Bottom right Digital Mic on Fluid and MTP.
	 * Digital Mic GM6 on CDP mainboard.
	 * Conncted to DMIC1 Input on Tabla codec.
	 */
	{"DMIC1", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic2"},

	/**
	 * Digital Mic3. Back Bottom Digital Mic on Fluid.
	 * Digital Mic GM1 on CDP mainboard.
	 * Conncted to DMIC4 Input on Tabla codec.
	 */
	{"DMIC4", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic3"},

	/**
	 * Digital Mic4. Back top Digital Mic on Fluid.
	 * Digital Mic GM2 on CDP mainboard.
	 * Conncted to DMIC3 Input on Tabla codec.
	 */
	{"DMIC3", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic4"},

	/**
	 * Digital Mic5. Front top Digital Mic on Fluid.
	 * Digital Mic GM3 on CDP mainboard.
	 * Conncted to DMIC5 Input on Tabla codec.
	 */
	{"DMIC5", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic5"},

	/* Tabla digital Mic6 - back bottom digital Mic on Liquid and
	 * bottom mic on CDP. FLUID/MTP do not have dmic6 installed.
	 */
	{"DMIC6", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic6"},
};

static const struct snd_soc_dapm_route comanche_audio_map_org[] = {
	{"Ext Spk Top Pos", NULL, "LINEOUT2"}
};

static const struct snd_soc_dapm_route express_audio_map_org[] = {
	{"Ext Spk Top Pos", NULL, "LINEOUT2"}
};

/* Main Main LDO is added */
static const struct snd_soc_dapm_widget msm8960_dapm_widgets_rev00[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8960_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_SPK("Ext Spk Top Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Top Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", msm8960_bias_event),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),

};

static const struct snd_soc_dapm_route common_audio_map_rev00[] = {
	{"EAR_RX_BIAS", NULL, "MCLK"},
	{"RX_BIAS", NULL, "MCLK"},
	{"LDO_H", NULL, "MCLK"},

	/* Speaker path */
	{"Ext Spk Bottom Pos", NULL, "LINEOUT1"},
	{"Ext Spk Bottom Neg", NULL, "LINEOUT5"},

	{"Ext Spk Top Pos", NULL, "LINEOUT3"},
	{"Ext Spk Top Neg", NULL, "LINEOUT4"},

	/* Microphone path */
	/**
	 *Samsung uses AMIC4 for Handset sub Mic
	 *AMIC4 uses external MIC BIAS1
	 */
	{"AMIC4", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Sub Mic"},

	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},

	/**
	 * AMIC3 and AMIC4 inputs are connected to ANC microphones
	 * These mics are biased differently on CDP and FLUID
	 * routing entries below are based on bias arrangement
	 * on FLUID.
	 */
	{"AMIC3", NULL, "Main Mic Bias"},
	{"Main Mic Bias", NULL, "Main Mic"},

	{"HEADPHONE", NULL, "LDO_H"},

	/**
	 * The digital Mic routes are setup considering
	 * fluid as default device.
	 */

	/**
	 * Digital Mic1. Front Bottom left Digital Mic on Fluid and MTP.
	 * Digital Mic GM5 on CDP mainboard.
	 * Conncted to DMIC2 Input on Tabla codec.
	 */
	{"DMIC2", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic1"},

	/**
	 * Digital Mic2. Front Bottom right Digital Mic on Fluid and MTP.
	 * Digital Mic GM6 on CDP mainboard.
	 * Conncted to DMIC1 Input on Tabla codec.
	 */
	{"DMIC1", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic2"},

	/**
	 * Digital Mic3. Back Bottom Digital Mic on Fluid.
	 * Digital Mic GM1 on CDP mainboard.
	 * Conncted to DMIC4 Input on Tabla codec.
	 */
	{"DMIC4", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic3"},

	/**
	 * Digital Mic4. Back top Digital Mic on Fluid.
	 * Digital Mic GM2 on CDP mainboard.
	 * Conncted to DMIC3 Input on Tabla codec.
	 */
	{"DMIC3", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic4"},

	/**
	 * Digital Mic5. Front top Digital Mic on Fluid.
	 * Digital Mic GM3 on CDP mainboard.
	 * Conncted to DMIC5 Input on Tabla codec.
	 */
	{"DMIC5", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic5"},

	/* Tabla digital Mic6 - back bottom digital Mic on Liquid and
	 * bottom mic on CDP. FLUID/MTP do not have dmic6 installed.
	 */
	{"DMIC6", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic6"},
};


/* VDD_CDC_RXTX , Third mic is added */
static const struct snd_soc_dapm_widget msm8960_dapm_widgets_rev01[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8960_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("VREG_CDC_RXTX", SND_SOC_NOPM, 0, 0,
	msm8960_cdc_vps_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Ext Spk Bottom Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_SPK("Ext Spk Top Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Top Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Third Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", msm8960_bias_event),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),

};

static const struct snd_soc_dapm_route common_audio_map_rev01[] = {
	{"EAR_RX_BIAS", NULL, "MCLK"},
	{"RX_BIAS", NULL, "MCLK"},
	{"LDO_H", NULL, "MCLK"},

	/* Speaker path */
	{"Ext Spk Bottom Pos", NULL, "LINEOUT1"},
	{"Ext Spk Bottom Neg", NULL, "LINEOUT5"},

	{"Ext Spk Top Pos", NULL, "LINEOUT2"},
	{"Ext Spk Top Neg", NULL, "LINEOUT4"},

	/* Microphone path */
	/**
	 *Samsung uses AMIC4 for Handset sub Mic
	 *AMIC4 uses external MIC BIAS1
	 */
	{"AMIC4", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Sub Mic"},

	{"AMIC5", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Third Mic"},

	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},

	/**
	 * AMIC3 and AMIC4 inputs are connected to ANC microphones
	 * These mics are biased differently on CDP and FLUID
	 * routing entries below are based on bias arrangement
	 * on FLUID.
	 */
	{"AMIC3", NULL, "Main Mic Bias"},
	{"Main Mic Bias", NULL, "Main Mic"},

	{"HEADPHONE", NULL, "LDO_H"},

	/**
	 * The digital Mic routes are setup considering
	 * fluid as default device.
	 */

	/**
	 * Digital Mic1. Front Bottom left Digital Mic on Fluid and MTP.
	 * Digital Mic GM5 on CDP mainboard.
	 * Conncted to DMIC2 Input on Tabla codec.
	 */
	{"DMIC2", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic1"},

	/**
	 * Digital Mic2. Front Bottom right Digital Mic on Fluid and MTP.
	 * Digital Mic GM6 on CDP mainboard.
	 * Conncted to DMIC1 Input on Tabla codec.
	 */
	{"DMIC1", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic2"},

	/**
	 * Digital Mic3. Back Bottom Digital Mic on Fluid.
	 * Digital Mic GM1 on CDP mainboard.
	 * Conncted to DMIC4 Input on Tabla codec.
	 */
	{"DMIC4", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic3"},

	/**
	 * Digital Mic4. Back top Digital Mic on Fluid.
	 * Digital Mic GM2 on CDP mainboard.
	 * Conncted to DMIC3 Input on Tabla codec.
	 */
	{"DMIC3", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic4"},

	/**
	 * Digital Mic5. Front top Digital Mic on Fluid.
	 * Digital Mic GM3 on CDP mainboard.
	 * Conncted to DMIC5 Input on Tabla codec.
	 */
	{"DMIC5", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic5"},

	/* Tabla digital Mic6 - back bottom digital Mic on Liquid and
	 * bottom mic on CDP. FLUID/MTP do not have dmic6 installed.
	 */
	{"DMIC6", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic6"},
};


static const char *spk_function[] = {"Off", "On"};
static const char *slim0_rx_ch_text[] = {"One", "Two"};
static const char *slim0_tx_ch_text[] = {"One", "Two", "Three", "Four"};
static char const *hdmi_rx_ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static const char * const hdmi_rate[] = {"Default", "Variable"};

static const struct soc_enum msm8960_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim0_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, hdmi_rate),
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_ch_text),
};

static const char *btsco_rate_text[] = {"8000", "16000"};
static const struct soc_enum msm8960_btsco_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};

static const char *auxpcm_rate_text[] = {"rate_8000", "rate_16000"};
static const struct soc_enum msm8960_auxpcm_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};
static int msm8960_i2s_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_i2s_rx_ch  = %d\n", __func__,
			msm8960_i2s_rx_ch);
	ucontrol->value.integer.value[0] = msm8960_i2s_rx_ch - 1;
	return 0;
}

static int msm8960_i2s_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8960_i2s_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm8960_i2s_rx_ch = %d\n", __func__,
			msm8960_i2s_rx_ch);
	return 1;
}

static int msm8960_i2s_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_i2s_tx_ch  = %d\n", __func__,
			msm8960_i2s_tx_ch);
	ucontrol->value.integer.value[0] = msm8960_i2s_tx_ch - 1;
	return 0;
}

static int msm8960_i2s_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8960_i2s_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm8960_i2s_tx_ch = %d\n", __func__,
			msm8960_i2s_tx_ch);
	return 1;
}

static int msm8960_slim_0_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_slim_0_rx_ch  = %d\n", __func__,
		 msm8960_slim_0_rx_ch);
	ucontrol->value.integer.value[0] = msm8960_slim_0_rx_ch - 1;
	return 0;
}

static int msm8960_slim_0_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8960_slim_0_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm8960_slim_0_rx_ch = %d\n", __func__,
		 msm8960_slim_0_rx_ch);
	return 1;
}

static int msm8960_slim_0_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_slim_0_tx_ch  = %d\n", __func__,
		 msm8960_slim_0_tx_ch);
	ucontrol->value.integer.value[0] = msm8960_slim_0_tx_ch - 1;
	return 0;
}

static int msm8960_slim_0_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8960_slim_0_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm8960_slim_0_tx_ch = %d\n", __func__,
		 msm8960_slim_0_tx_ch);
	return 1;
}

static int msm8960_btsco_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_btsco_rate  = %d", __func__,
					msm8960_btsco_rate);
	ucontrol->value.integer.value[0] = msm8960_btsco_rate;
	return 0;
}

static int msm8960_btsco_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 8000:
		msm8960_btsco_rate = SAMPLE_RATE_8KHZ;
		break;
	case 16000:
		msm8960_btsco_rate = SAMPLE_RATE_16KHZ;
		break;
	default:
		msm8960_btsco_rate = SAMPLE_RATE_8KHZ;
		break;
	}
	pr_debug("%s: msm8960_btsco_rate = %d\n", __func__, msm8960_btsco_rate);
	return 0;
}

static int msm8960_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_auxpcm_rate  = %d", __func__,
		msm8960_auxpcm_rate);
	ucontrol->value.integer.value[0] = msm8960_auxpcm_rate;
	return 0;
}

static int msm8960_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8960_auxpcm_rate = SAMPLE_RATE_8KHZ;
		break;
	case 1:
		msm8960_auxpcm_rate = SAMPLE_RATE_16KHZ;
		break;
	default:
		msm8960_auxpcm_rate = SAMPLE_RATE_8KHZ;
		break;
	}
	pr_debug("%s: msm8960_auxpcm_rate = %d\n", __func__,
		msm8960_auxpcm_rate);
	return 0;
}

static int msm8960_i2s_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8960_i2s_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm8960_i2s_spk_control = ucontrol->value.integer.value[0];
	msm8960_ext_control(codec);
	return 1;
}

static int msm8960_i2s_get_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_spk_control = %d", __func__, msm8960_spk_control);
	ucontrol->value.integer.value[0] = msm8960_i2s_spk_control;
	return 0;
}

static const struct snd_kcontrol_new tabla_msm8960_i2s_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm8960_enum[0], msm8960_i2s_get_spk,
		msm8960_i2s_set_spk),
	SOC_ENUM_EXT("PRI_RX Channels", msm8960_enum[1],
		msm8960_i2s_rx_ch_get, msm8960_i2s_rx_ch_put),
	SOC_ENUM_EXT("PRI_TX Channels", msm8960_enum[2],
		msm8960_i2s_tx_ch_get, msm8960_i2s_tx_ch_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm8960_btsco_enum[0],
		msm8960_btsco_rate_get, msm8960_btsco_rate_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", msm8960_auxpcm_enum[0],
		msm8960_auxpcm_rate_get, msm8960_auxpcm_rate_put),
};
static int msm_hdmi_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hdmi_rx_ch  = %d\n", __func__,
			msm_hdmi_rx_ch);
	ucontrol->value.integer.value[0] = msm_hdmi_rx_ch - 2;
	return 0;
}

static int msm_hdmi_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_hdmi_rx_ch = ucontrol->value.integer.value[0] + 2;

	pr_debug("%s: msm_hdmi_rx_ch = %d\n", __func__,
		msm_hdmi_rx_ch);
	return 1;
}

static int msm8960_hdmi_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	hdmi_rate_variable = ucontrol->value.integer.value[0];
	pr_debug("%s: hdmi_rate_variable = %d\n", __func__, hdmi_rate_variable);
	return 0;
}

static int msm8960_hdmi_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = hdmi_rate_variable;
	return 0;
}

static const struct snd_kcontrol_new tabla_msm8960_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm8960_enum[0], msm8960_get_spk,
		msm8960_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm8960_enum[1],
		msm8960_slim_0_rx_ch_get, msm8960_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm8960_enum[2],
		msm8960_slim_0_tx_ch_get, msm8960_slim_0_tx_ch_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm8960_btsco_enum[0],
		msm8960_btsco_rate_get, msm8960_btsco_rate_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", msm8960_auxpcm_enum[0],
		msm8960_auxpcm_rate_get, msm8960_auxpcm_rate_put),
	SOC_ENUM_EXT("HDMI RX Rate", msm8960_enum[3],
					msm8960_hdmi_rate_get,
					msm8960_hdmi_rate_put),
	SOC_ENUM_EXT("HDMI_RX Channels", msm8960_enum[4],
		msm_hdmi_rx_ch_get, msm_hdmi_rx_ch_put),
};

static void *def_tabla_mbhc_cal(void)
{
	void *tabla_cal;
	struct tabla_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	tabla_cal = kzalloc(TABLA_MBHC_CAL_SIZE(TABLA_MBHC_DEF_BUTTONS,
						TABLA_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!tabla_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((TABLA_MBHC_CAL_GENERAL_PTR(tabla_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_PLUG_DET_PTR(tabla_cal)->X) = (Y))
	S(mic_current, TABLA_PID_MIC_5_UA);
	S(hph_current, TABLA_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_PLUG_TYPE_PTR(tabla_cal)->X) = (Y))
	S(v_no_mic, 590);
	S(v_hs_max, 1650);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 4);
	S(n_btn_con, 5);
	S(num_btn, TABLA_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal);
	btn_low = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_LOW);
	btn_high = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_HIGH);
	if (machine_is_M2_VZW() && system_rev >= BOARD_REV10) {
		btn_low[0] = -105;
		btn_high[0] = 65;
		btn_low[1] = 66;
		btn_high[1] = 260;
		btn_low[2] = 261;
		btn_high[2] = 500;
	} else {
		btn_low[0] = -100;
		btn_high[0] = 75;
		btn_low[1] = 76;
		btn_high[1] = 200;
		btn_low[2] = 201;
		btn_high[2] = 500;
	}
	n_ready = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_N_READY);
	n_ready[0] = 48;
	n_ready[1] = 38;
	n_cic = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return tabla_cal;
}

static int msm8960_i2s_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_debug("%s()\n", __func__);
	err = snd_soc_add_codec_controls(codec, tabla_msm8960_i2s_controls,
				ARRAY_SIZE(tabla_msm8960_i2s_controls));
	if (err < 0) {
		pr_err("returning loc 1 err = %d\n", err);
		return err;
	}
	rtd->pmdown_time = 0;

	if (machine_is_COMANCHE() || machine_is_EXPRESS() ||
		(machine_is_JASPER() && system_rev < BOARD_REV02))
		snd_soc_dapm_new_controls(dapm, msm8960_dapm_widgets_org,
			ARRAY_SIZE(msm8960_dapm_widgets_org));
	else
		snd_soc_dapm_new_controls(dapm, msm8960_dapm_widgets_rev01,
			ARRAY_SIZE(msm8960_dapm_widgets_rev01));
	if (machine_is_COMANCHE() || machine_is_EXPRESS() ||
		(machine_is_JASPER() && system_rev < BOARD_REV02))
		snd_soc_dapm_add_routes(dapm, common_audio_map_org,
			ARRAY_SIZE(common_audio_map_org));
	else
		snd_soc_dapm_add_routes(dapm, common_audio_map_rev01,
			ARRAY_SIZE(common_audio_map_rev01));

	if (machine_is_COMANCHE())
		snd_soc_dapm_add_routes(dapm, comanche_audio_map_org,
			ARRAY_SIZE(comanche_audio_map_org));

	if (machine_is_EXPRESS())
		snd_soc_dapm_add_routes(dapm, express_audio_map_org,
			ARRAY_SIZE(express_audio_map_org));

	snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Neg");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Neg");

	snd_soc_dapm_sync(dapm);

	err = snd_soc_jack_new(codec, "Headset Jack",
			       (SND_JACK_HEADSET | SND_JACK_OC_HPHL |
				SND_JACK_OC_HPHR),
				&hs_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Button Jack",
			SND_JACK_BTN_0, &button_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Volumeup Jack",
			SND_JACK_BTN_1, &volumeup_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Volumedown Jack",
			SND_JACK_BTN_2, &volumedown_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	/* BTN_1 button is mapped to VOLUME Up key type*/
	snd_jack_set_key(volumeup_jack.jack,
			SND_JACK_BTN_1, KEY_VOLUMEUP);
	/* BTN_2 button is mapped to VOLUME Down key type*/
	snd_jack_set_key(volumedown_jack.jack,
			SND_JACK_BTN_2, KEY_VOLUMEDOWN);

	if (!(machine_is_M2_ATT() && system_rev >= BOARD_REV10) &&
		!(machine_is_M2_VZW() && system_rev >= BOARD_REV13) &&
		!(machine_is_M2_SPR() && system_rev >= BOARD_REV08) &&
		!(machine_is_AEGIS2() && system_rev >= BOARD_REV04) &&
		!(machine_is_JASPER() && system_rev >= BOARD_REV04) &&
		!(machine_is_COMANCHE() && system_rev >= BOARD_REV02) &&
		!(machine_is_GOGH() && system_rev >= BOARD_REV03) &&
		!(machine_is_INFINITE() && system_rev >= BOARD_REV03) &&
		!(machine_is_EXPRESS() && system_rev >= BOARD_REV02)) {
		/* using mbhc driver for earjack */
			tabla_hs_detect(codec, &mbhc_cfg);
	}

	return 0;
}


static int msm8960_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		pr_debug("%s: rx_0_ch=%d\n", __func__, msm8960_slim_0_rx_ch);

		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				msm8960_slim_0_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(codec_dai, 0, 0,
				msm8960_slim_0_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set codec channel map\n",
								__func__);
			goto end;
		}
	} else {

		pr_debug("%s: %s  tx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, msm8960_slim_0_tx_ch);

		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				msm8960_slim_0_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(codec_dai,
				msm8960_slim_0_tx_ch, tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set codec channel map\n",
								__func__);
			goto end;
		}
	}
end:
	return ret;
}
/*
static int msm8960_slimbus_2_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;
	unsigned int num_tx_ch = 0;
	unsigned int num_rx_ch = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		num_rx_ch =  params_channels(params);

		pr_debug("%s: %s rx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_rx_ch);

		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				num_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(codec_dai, 0, 0,
				num_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set codec channel map\n",
								__func__);
			goto end;
		}
	} else {
		num_tx_ch =  params_channels(params);

		pr_debug("%s: %s  tx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_tx_ch);

		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
				num_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(codec_dai,
				num_tx_ch, tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set codec channel map\n",
								__func__);
			goto end;
		}
	}
end:
	return ret;
}
*/
static int msm8960_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct pm_gpio jack_gpio_cfg = {
		.direction = PM_GPIO_DIR_IN,
		.pull = PM_GPIO_PULL_UP_1P5,
		.function = PM_GPIO_FUNC_NORMAL,
		.vin_sel = 2,
		.inv_int_pol = 0,
	};

	pr_debug("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	if (machine_is_msm8960_liquid()) {
		top_spk_pamp_gpio = (PM8921_GPIO_PM_TO_SYS
					(PMIC_GPIO_VPS_EN));
		bottom_spk_pamp_gpio = (PM8921_GPIO_PM_TO_SYS
					(PMIC_GPIO_SPK_EN));
	}
	rtd->pmdown_time = 0;

	snd_soc_dapm_new_controls(dapm, msm8960_dapm_widgets_rev00,
				ARRAY_SIZE(msm8960_dapm_widgets_rev00));
	if ((machine_is_M2_SKT() && system_rev >= BOARD_REV01)) {
		snd_soc_dapm_add_routes(dapm, common_audio_map_rev00,
			ARRAY_SIZE(common_audio_map_rev00));
	} else if (machine_is_STRETTO()) {
		snd_soc_dapm_add_routes(dapm, common_audio_map_rev00,
			ARRAY_SIZE(common_audio_map_rev00));
	} else if (machine_is_SUPERIORLTE_SKT()) {
		snd_soc_dapm_add_routes(dapm, common_audio_map_rev00,
			ARRAY_SIZE(common_audio_map_rev00));
	} else if (machine_is_M2_DCM()) {
		snd_soc_dapm_add_routes(dapm, common_audio_map_rev00,
			ARRAY_SIZE(common_audio_map_rev00));
	} else if (machine_is_K2_KDI()) {
		snd_soc_dapm_add_routes(dapm, common_audio_map_rev00,
			ARRAY_SIZE(common_audio_map_rev00));
	} else {
	snd_soc_dapm_add_routes(dapm, common_audio_map_org,
				ARRAY_SIZE(common_audio_map_org));
	}

	snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Neg");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Neg");

	snd_soc_dapm_sync(dapm);

	err = snd_soc_jack_new(codec, "Headset Jack",
			       (SND_JACK_HEADSET | SND_JACK_LINEOUT |
				SND_JACK_OC_HPHL | SND_JACK_OC_HPHR |
				SND_JACK_UNSUPPORTED),
				&hs_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}

	err = snd_soc_jack_new(codec, "Button Jack",
		       SND_JACK_BTN_0, &button_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Volumeup Jack",
			SND_JACK_BTN_1, &volumeup_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Volumedown Jack",
			SND_JACK_BTN_2, &volumedown_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	/* BTN_1 button is mapped to VOLUME Up key type*/
	snd_jack_set_key(volumeup_jack.jack,
			SND_JACK_BTN_1, KEY_VOLUMEUP);
	/* BTN_2 button is mapped to VOLUME Down key type*/
	snd_jack_set_key(volumedown_jack.jack,
			SND_JACK_BTN_2, KEY_VOLUMEDOWN);

	if (((machine_is_M2_SKT() && system_rev < BOARD_REV08) ||
		(machine_is_M2_DCM() && system_rev < BOARD_REV03) ||
		(machine_is_K2_KDI() && system_rev < BOARD_REV03)) ||
		(!machine_is_M2_SKT() && !machine_is_M2_DCM() &&
		!machine_is_STRETTO() && !machine_is_K2_KDI() &&
		!machine_is_SUPERIORLTE_SKT())) {
		/* using mbhc driver for earjack */
		if (GPIO_DETECT_USED) {
			mbhc_cfg.gpio = PM8921_GPIO_PM_TO_SYS(JACK_DETECT_GPIO);
			mbhc_cfg.gpio_irq = JACK_DETECT_INT;
		}
		
		if (mbhc_cfg.gpio) {
			err = pm8xxx_gpio_config(mbhc_cfg.gpio, &jack_gpio_cfg);
			if (err) {
				pr_err("%s: pm8xxx_gpio_config failed %d\n", __func__,
					   err);
				return err;
			}
		}
		err = tabla_hs_detect(codec, &mbhc_cfg);
	}

	return err;
}
static int msm8960_i2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8960_i2s_rx_ch;

	return 0;
}

static int msm8960_i2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8960_i2s_tx_ch;

	return 0;
}

static int msm8960_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8960_slim_0_rx_ch;

	return 0;
}

static int msm8960_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8960_slim_0_tx_ch;

	return 0;
}

static int msm8960_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}
#if !defined(CONFIG_MACH_COMANCHE) && !defined (CONFIG_MACH_GOGH) && !defined (CONFIG_MACH_JASPER)
static int msm8960_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
			channels->min, channels->max);

	if (!hdmi_rate_variable)
		rate->min = rate->max = 48000;
	channels->min = channels->max = msm_hdmi_rx_ch;
	if (channels->max < 2)
		channels->min = channels->max = 2;

	return 0;
}
#endif
static int msm8960_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8960_btsco_rate;
	channels->min = channels->max = msm8960_btsco_ch;

	return 0;
}
static int msm8960_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8960_auxpcm_rate;
	/* PCM only supports mono output */
	channels->min = channels->max = 1;

	return 0;
}
static int msm8960_proxy_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}
static int msm8960_aux_pcm_get_gpios(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	ret = gpio_request(GPIO_AUX_PCM_DOUT, "AUX PCM DOUT");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM DOUT",
				__func__, GPIO_AUX_PCM_DOUT);
		goto fail_dout;
	}

	ret = gpio_request(GPIO_AUX_PCM_DIN, "AUX PCM DIN");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM DIN",
				__func__, GPIO_AUX_PCM_DIN);
		goto fail_din;
	}

	ret = gpio_request(GPIO_AUX_PCM_SYNC, "AUX PCM SYNC");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM SYNC",
				__func__, GPIO_AUX_PCM_SYNC);
		goto fail_sync;
	}
	ret = gpio_request(GPIO_AUX_PCM_CLK, "AUX PCM CLK");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM CLK",
				__func__, GPIO_AUX_PCM_CLK);
		goto fail_clk;
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_DOUT, 1, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_DIN, 1, GPIO_CFG_INPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_SYNC, 1, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_CLK, 1, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	return 0;

fail_clk:
	gpio_free(GPIO_AUX_PCM_SYNC);
fail_sync:
	gpio_free(GPIO_AUX_PCM_DIN);
fail_din:
	gpio_free(GPIO_AUX_PCM_DOUT);
fail_dout:

	return ret;
}

static int msm8960_aux_pcm_free_gpios(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_DOUT, 0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_DIN, 0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_SYNC, 0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_AUX_PCM_CLK, 0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);

	gpio_free(GPIO_AUX_PCM_DIN);
	gpio_free(GPIO_AUX_PCM_DOUT);
	gpio_free(GPIO_AUX_PCM_SYNC);
	gpio_free(GPIO_AUX_PCM_CLK);

	return 0;
}
static int msm8960_cdc_i2s_rx_free_gpios(void)
{
	gpio_free(GPIO_SPKR_I2S_RX_SCK);
	gpio_free(GPIO_SPKR_I2S_RX_DOUT);
	gpio_free(GPIO_SPKR_I2S_RX_WS);

	return 0;
}

static int msm8960_cdc_i2s_tx_free_gpios(void)
{
	gpio_free(GPIO_SPKR_I2S_TX_SCK);
	gpio_free(GPIO_SPKR_I2S_TX_D0);
	gpio_free(GPIO_SPKR_I2S_TX_WS);

	return 0;
}

static int msm8660_i2s_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	int rate = params_rate(params);
	int bit_clk_set = 0;
	pr_info("%s Codec Clock 0x%x ; Rx Bit Clock %x\n", __func__,
			(unsigned int)codec_clk, (unsigned int)rx_bit_clk);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		bit_clk_set = I2S_MCLK_RATE / (rate * 2 *
						NO_OF_BITS_PER_SAMPLE);
		clk_set_rate(rx_bit_clk, bit_clk_set);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		bit_clk_set = I2S_MIC_MCLK_RATE / (rate * 2 *
						NO_OF_BITS_PER_SAMPLE);
		clk_set_rate(tx_bit_clk, bit_clk_set);
	}
	return 1;
}


static void msm8960_i2s_shutdown(struct snd_pcm_substream *substream)
{
	pr_info("%s Codec Clock 0x%x ; Rx Bit Clock %x\n", __func__,
			(unsigned int)codec_clk, (unsigned int)rx_bit_clk);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rx_bit_clk) {
			clk_disable_unprepare(rx_bit_clk);
			clk_put(rx_bit_clk);
			clk_disable_unprepare(codec_clk);
			clk_put(codec_clk);
			rx_bit_clk = NULL;
		}
		msm8960_cdc_i2s_rx_free_gpios();
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (tx_bit_clk) {
			clk_disable_unprepare(tx_bit_clk);
			clk_put(tx_bit_clk);
			clk_disable_unprepare(tx_osr_clk);
			clk_put(tx_osr_clk);
			tx_bit_clk = NULL;
		}
		msm8960_cdc_i2s_tx_free_gpios();
	}
}

int configure_i2s_rx_gpio(void)
{
	u8 ret = 0;

	ret = gpio_request(GPIO_SPKR_I2S_RX_SCK, "I2S_RX_SCK");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
				GPIO_SPKR_I2S_RX_SCK);
		goto err;
	}
	ret = gpio_request(GPIO_SPKR_I2S_RX_DOUT, "I2S_RX_DOUT");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
				GPIO_SPKR_I2S_RX_DOUT);
		goto err;
	}
	ret = gpio_request(GPIO_SPKR_I2S_RX_WS, "I2S_RX_WS");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
			GPIO_SPKR_I2S_RX_WS);
		goto err;
	}
err:
	return ret;
}

int configure_i2s_tx_gpio(void)
{
	u8 ret = 0;

	ret = gpio_request(GPIO_SPKR_I2S_TX_SCK, "I2S_RX_SCK");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
				GPIO_SPKR_I2S_TX_SCK);
		goto err;
	}
	ret = gpio_request(GPIO_SPKR_I2S_TX_D0, "I2S_RX_DOUT");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
				GPIO_SPKR_I2S_TX_D0);
		goto err;
	}
	ret = gpio_request(GPIO_SPKR_I2S_TX_WS, "I2S_RX_WS");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
			GPIO_SPKR_I2S_TX_WS);
		goto err;
	}
err:
	return ret;
}

static int msm8960_i2s_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	pr_info("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		configure_i2s_rx_gpio();
	codec_clk = clk_get(cpu_dai->dev, "osr_clk");
	if (IS_ERR(codec_clk)) {
		pr_err("%s Error getting codec clk 0x%x\n", __func__ ,
			(unsigned int)codec_clk);
		return -EBUSY;
	}
	clk_set_rate(codec_clk, TABLA_EXT_CLK_RATE);
	ret = clk_prepare_enable(codec_clk);
	if (ret) {
		pr_err("Unable to enable codec clock\n");
		clk_put(codec_clk);
		return ret;
	}

	rx_bit_clk = clk_get(cpu_dai->dev, "bit_clk");
	if (IS_ERR(rx_bit_clk)) {
		pr_err("Failed to get bit_clk\n");
		clk_disable(codec_clk);
		clk_put(codec_clk);
		return -EBUSY;
	}
	clk_set_rate(rx_bit_clk, 8);
	ret = clk_prepare_enable(rx_bit_clk);
	if (ret) {
		pr_err("Unable to enable bit_clk\n");
		clk_disable(codec_clk);
		clk_put(codec_clk);
		clk_put(rx_bit_clk);
		return ret;
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		pr_err("set format for cpu dai failed\n");
		return ret;
	}
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		pr_err("set format for codec dai failed\n");
		return ret;
	}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		configure_i2s_tx_gpio();

		tx_osr_clk = clk_get(cpu_dai->dev, "osr_clk");
		if (IS_ERR(tx_osr_clk)) {
			pr_debug("Failed to get osr_clk\n");
			return -EBUSY;
		}
		/* Master clock OSR 256 */
		clk_set_rate(tx_osr_clk, I2S_MIC_MCLK_RATE);
		ret = clk_prepare_enable(tx_osr_clk);
		if (ret) {
			pr_debug("Unable to enable osr_clk\n");
			clk_put(tx_osr_clk);
			return ret;
		}
		tx_bit_clk = clk_get(cpu_dai->dev, "bit_clk");
		if (IS_ERR(tx_bit_clk)) {
			pr_debug("Failed to get bit_clk\n");
			clk_disable(tx_osr_clk);
			clk_put(tx_osr_clk);
			return -EBUSY;
		}
		clk_set_rate(tx_bit_clk, 8);
		ret = clk_prepare_enable(tx_bit_clk);
		if (ret) {
			pr_debug("Unable to enable bit_clk\n");
			clk_put(tx_bit_clk);
			clk_disable(tx_osr_clk);
			clk_put(tx_osr_clk);
			return ret;
		}
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);

		if (ret < 0) {
			pr_err("set format for cpu dai failed\n");
			return ret;
		}
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0) {
			pr_err("set format for codec dai failed\n");
			return ret;
		}
	}
	return ret;
}

static int msm8960_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	pr_debug("%s(): dai_link_str_name = %s cpu_dai = %s codec_dai = %s\n",
		__func__, rtd->dai_link->stream_name,
		rtd->dai_link->cpu_dai_name, rtd->dai_link->codec_dai_name);
	return 0;
}

static int msm8960_auxpcm_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s(): substream = %s, auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&auxpcm_rsc_ref));
	if (atomic_inc_return(&auxpcm_rsc_ref) == 1)
		ret = msm8960_aux_pcm_get_gpios();

	if (ret < 0) {
		pr_err("%s: Aux PCM GPIO request failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static void msm8960_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s, auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&auxpcm_rsc_ref));
	if (atomic_dec_return(&auxpcm_rsc_ref) == 0)
		msm8960_aux_pcm_free_gpios();
}

static void msm8960_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	pr_debug("%s(): dai_link str_name = %s cpu_dai = %s codec_dai = %s\n",
		__func__, rtd->dai_link->stream_name,
		rtd->dai_link->cpu_dai_name, rtd->dai_link->codec_dai_name);
}

static struct snd_soc_ops msm8960_be_ops = {
	.startup = msm8960_startup,
	.hw_params = msm8960_hw_params,
	.shutdown = msm8960_shutdown,
};

static struct snd_soc_ops msm8960_i2s_be_ops = {
	.startup = msm8960_i2s_startup,
	.shutdown = msm8960_i2s_shutdown,
	.hw_params = msm8660_i2s_hw_params,
};

static struct snd_soc_ops msm8960_auxpcm_be_ops = {
	.startup = msm8960_auxpcm_startup,
	.shutdown = msm8960_auxpcm_shutdown,
};

static struct snd_soc_dai_link *msm8960_dai_list;

static struct snd_soc_dai_link msm8960_i2s_be_dai[] = {
	{
		.name = LPASS_BE_PRI_I2S_RX,
		.stream_name = "Primary I2S Playback",
		.cpu_dai_name = "msm-dai-q6.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_I2S_RX,
		.init = &msm8960_i2s_audrx_init,
		.be_hw_params_fixup = msm8960_i2s_rx_be_hw_params_fixup,
		.ops = &msm8960_i2s_be_ops,
	},
	{
		.name = LPASS_BE_PRI_I2S_TX,
		.stream_name = "Primary I2S Capture",
		.cpu_dai_name = "msm-dai-q6.1",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_I2S_TX,
		.be_hw_params_fixup = msm8960_i2s_tx_be_hw_params_fixup,
		.ops = &msm8960_i2s_be_ops,
	},
};

static struct snd_soc_dai_link msm8960_slimbus_be_dai[] = {
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm8960_audrx_init,
		.be_hw_params_fixup = msm8960_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8960_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm8960_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8960_be_ops,
	},
};

/*
static struct snd_soc_ops msm8960_slimbus_2_be_ops = {
	.startup = msm8960_startup,
	.hw_params = msm8960_slimbus_2_hw_params,
	.shutdown = msm8960_shutdown,
};
*/
/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8960_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8960 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8960 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name	= "MultiMedia2",
		.platform_name  = "msm-multi-ch-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "MSM8960 LPA",
		.stream_name = "LPA",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PMC purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name	= "SLIMBUS0_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{
		.name = "MSM8960 Compr",
		.stream_name = "COMPR",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compr-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name	= "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOLTE,
	},
	{
		.name = "Voice2",
		.stream_name = "Voice2",
		.cpu_dai_name   = "Voice2",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
					SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,/* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICE2,
	},
	{
		.name = "MSM8960 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name	= "MultiMedia5",
		.platform_name  = "msm-lowlatency-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
					SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	/* Backend BT/FM DAI Links */
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = msm8960_btsco_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = msm8960_btsco_be_hw_params_fixup,
	},
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_RX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_INT_FM_TX,
		.stream_name = "Internal FM Capture",
		.cpu_dai_name = "msm-dai-q6.12293",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_TX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
	},
#ifdef CONFIG_SND_SOC_MSM_QDSP6_HDMI_AUDIO
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm8960_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
#endif
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm8960_proxy_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm8960_proxy_be_hw_params_fixup,
	},
	/* AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm8960_auxpcm_be_params_fixup,
		.ops = &msm8960_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm8960_auxpcm_be_params_fixup,
		.ops = &msm8960_auxpcm_be_ops,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
};

static struct snd_soc_card snd_soc_card_msm8960 = {
		.name		= "msm8960-snd-card",
		.dai_link	= msm8960_dai,
		.num_links	= ARRAY_SIZE(msm8960_dai),
		.controls = tabla_msm8960_controls,
		.num_controls = ARRAY_SIZE(tabla_msm8960_controls),
};

static struct platform_device *msm8960_snd_device;

static int msm8960_configure_audio_gpios(void)
{
	int ret;
	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull	   = PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};
#if !defined(CONFIG_MACH_M2_DCM) && !defined(CONFIG_MACH_AEGIS2) \
	&& !defined(CONFIG_MACH_K2_KDI) && !defined(CONFIG_MACH_EXPRESS)
	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(23), "AV_SWITCH");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(23));
		return ret;
	}

	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(23), &param);
	if (ret) {
		pr_err("%s: Failed to configure gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(23));
		return ret;
	}
	gpio_direction_output(PM8921_GPIO_PM_TO_SYS(23), 0);

	ret = gpio_request(us_euro_sel_gpio, "US_EURO_SWITCH");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
		       us_euro_sel_gpio);
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		return ret;
	}
	ret = pm8xxx_gpio_config(us_euro_sel_gpio, &param);
	if (ret) {
		pr_err("%s: Failed to configure gpio %d\n", __func__,
		       us_euro_sel_gpio);
		return ret;
	}
	gpio_direction_output(us_euro_sel_gpio, 0);
#endif
#if !defined(CONFIG_MACH_AEGIS2)
	ret = gpio_request(bottom_spk_pamp_gpio, "BOTTOM_SPK_AMP");
	if (ret) {
		pr_err("%s: Error requesting BOTTOM SPK AMP GPIO %u\n",
			__func__, bottom_spk_pamp_gpio);
	}
	ret = pm8xxx_gpio_config(bottom_spk_pamp_gpio, &param);
	if (ret) {
		pr_err("%s: Failed to configure Bottom Spk Ampl"
			" gpio %u\n", __func__, bottom_spk_pamp_gpio);
		return ret;
	} else
		gpio_direction_output(bottom_spk_pamp_gpio, 0);
#endif
	ret = gpio_request(top_spk_pamp_gpio, "TOP_SPK_AMP");
	if (ret) {
		pr_err("%s: Error requesting TOP SPK AMP GPIO %u\n",
			__func__, top_spk_pamp_gpio);
	}
	ret = pm8xxx_gpio_config(top_spk_pamp_gpio, &param);
	if (ret) {
		pr_err("%s: Failed to configure Top Spk Ampl"
			" gpio %u\n", __func__, top_spk_pamp_gpio);
		return ret;
	} else
		gpio_direction_output(top_spk_pamp_gpio, 0);

	return 0;
}
static void msm8960_free_audio_gpios(void)
{
	if (msm8960_audio_gpios_configured) {
#if !defined(CONFIG_MACH_M2_DCM) && !defined(CONFIG_MACH_AEGIS2) \
	&& !defined(CONFIG_MACH_K2_KDI) && !defined(CONFIG_MACH_EXPRESS)
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		gpio_free(PM8921_GPIO_PM_TO_SYS(35));
#endif
		gpio_free(top_spk_pamp_gpio);
#if !defined(CONFIG_MACH_AEGIS2)
		gpio_free(bottom_spk_pamp_gpio);
#endif
	}
}

static int __init msm8960_audio_init(void)
{
	int ret;
	msm8960_dai_list = kzalloc(sizeof(msm8960_dai) +
			2 * sizeof(struct snd_soc_dai_link), GFP_KERNEL);
	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		memcpy(msm8960_dai_list, msm8960_dai, sizeof(msm8960_dai));
		memcpy(&msm8960_dai_list[ARRAY_SIZE(msm8960_dai)],
			msm8960_slimbus_be_dai, sizeof(msm8960_slimbus_be_dai));
		snd_soc_card_msm8960.dai_link = msm8960_dai_list;
		snd_soc_card_msm8960.num_links = ARRAY_SIZE(msm8960_dai) +
					ARRAY_SIZE(msm8960_slimbus_be_dai);
	}else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C) {
		memcpy(msm8960_dai_list, msm8960_dai, sizeof(msm8960_dai));
		memcpy(&msm8960_dai_list[ARRAY_SIZE(msm8960_dai)],
				msm8960_i2s_be_dai, sizeof(msm8960_i2s_be_dai));
		snd_soc_card_msm8960.dai_link = msm8960_dai_list;
		snd_soc_card_msm8960.num_links = ARRAY_SIZE(msm8960_dai) +
					ARRAY_SIZE(msm8960_i2s_be_dai);
	}

	mbhc_cfg.calibration = def_tabla_mbhc_cal();
	if (!mbhc_cfg.calibration) {
		pr_err("Calibration data allocation failed\n");
		kfree(msm8960_dai_list);
		return -ENOMEM;
	}

	msm8960_snd_device = platform_device_alloc("soc-audio", 0);
	if (!msm8960_snd_device) {
		pr_err("Platform device allocation failed\n");
		kfree(mbhc_cfg.calibration);
		kfree(msm8960_dai_list);
		return -ENOMEM;
	}
	platform_set_drvdata(msm8960_snd_device, &snd_soc_card_msm8960);
	ret = platform_device_add(msm8960_snd_device);
	if (ret) {
		platform_device_put(msm8960_snd_device);
		kfree(mbhc_cfg.calibration);
		kfree(msm8960_dai_list);
		return ret;
	}

	if (cpu_is_msm8960()) {
	if (msm8960_configure_audio_gpios()) {
		pr_err("%s Fail to configure headset mic gpios\n", __func__);
		msm8960_audio_gpios_configured = 0;
	} else
		msm8960_audio_gpios_configured = 1;
	} else {
		msm8960_audio_gpios_configured = 0;
	}

	mutex_init(&cdc_mclk_mutex);
	atomic_set(&auxpcm_rsc_ref, 0);
	
	INIT_DELAYED_WORK(&ext_amp_dwork.dwork,
			external_speaker_amp_work);
#if defined(CONFIG_MACH_AEGIS2)
	INIT_DELAYED_WORK(&bottom_amp_dwork.dwork,
			bottom_speaker_amp_work);
#endif
	return ret;

}
module_init(msm8960_audio_init);

static void __exit msm8960_audio_exit(void)
{
	msm8960_free_audio_gpios();
	kfree(msm8960_dai_list);
	platform_device_unregister(msm8960_snd_device);
	kfree(mbhc_cfg.calibration);
	mutex_destroy(&cdc_mclk_mutex);
}
module_exit(msm8960_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MSM8960");
MODULE_LICENSE("GPL v2");
