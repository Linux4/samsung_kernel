/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <sound/soc-dsp.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include "msm-pcm-routing.h"
#include "../codecs/wcd9310.h"
#include <linux/mfd/wcd9310/core.h>
#include <mach/msm8960-gpio.h>

/* 8960 machine driver */

#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_IRQ_BASE (NR_MSM_IRQS + NR_GPIO_IRQS)
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)

#define MSM8960_SPK_ON 1
#define MSM8960_SPK_OFF 0

#define msm8960_SLIM_0_RX_MAX_CHANNELS		2
#define msm8960_SLIM_0_TX_MAX_CHANNELS		4

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

#define BOTTOM_SPK_AMP_POS	0x1
#define BOTTOM_SPK_AMP_NEG	0x2
#define TOP_SPK_AMP_POS		0x4
#define TOP_SPK_AMP_NEG		0x8

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

#define TABLA_MBHC_DEF_BUTTONS 3
#define TABLA_MBHC_DEF_RLOADS 5

#ifndef BOARD_REV16
#define BOARD_REV16 (BOARD_REV15 + 1)
#endif

#define JACK_DETECT_GPIO 38
#define JACK_DETECT_INT PM8921_GPIO_IRQ(PM8921_IRQ_BASE, JACK_DETECT_GPIO)
#define GPIO_DETECT_USED false

#if defined(CONFIG_MACH_APEXQ)
static u32 top_spk_pamp_gpio;
#else
static u32 top_spk_pamp_gpio  = PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_SPK_EN);
#endif
static u32 bottom_spk_pamp_gpio = PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_VPS_EN);

#if defined(CONFIG_MACH_JAGUAR)
static u32 ear_switch_gpio  = PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_OUT_SEL);
#endif

static int msm8960_spk_control;
static int msm8960_ext_bottom_spk_pamp;
static int msm8960_ext_top_spk_pamp;
static int msm8960_slim_0_rx_ch = 1;
static int msm8960_slim_0_tx_ch = 1;
static int msm8960_slim_0_sample_rate = 48000;


static int msm8960_btsco_rate = BTSCO_RATE_8KHZ;
static int msm8960_btsco_ch = 1;

static struct clk *codec_clk;
static int clk_users;

static int msm8960_headset_gpios_configured;

static struct snd_soc_jack hs_jack;
static struct snd_soc_jack button_jack;
static struct snd_soc_jack volumedown_jack;
static struct snd_soc_jack volumeup_jack;

static int msm8960_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);

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
};

static int msm8960_i2s_rx_ch = 1;
static int msm8960_i2s_tx_ch = 1;
static int msm8960_i2s_spk_control;
static struct clk *rx_osr_clk;
static struct clk *rx_bit_clk;
static struct clk *tx_osr_clk;
static struct clk *tx_bit_clk;

static struct mutex cdc_mclk_mutex;

struct ext_amp_work {
	struct delayed_work dwork;
};

static struct ext_amp_work ext_amp_dwork;

/* Work queue for delaying the amp power on-off to
remove the static noise during SPK_PA enable */

static void external_speaker_amp_work(struct work_struct *work)
{
	pr_info("kth %s :: Top Speaker Amp enable\n", __func__);
	gpio_direction_output(top_spk_pamp_gpio, 1);
	pr_info("%s: slepping 4 ms after turning on external "
			" Top Speaker Ampl\n", __func__);
	usleep_range(4000, 4000);
}

static void msm8960_enable_ext_spk_amp_gpio(u32 spk_amp_gpio)
{
	if (spk_amp_gpio == bottom_spk_pamp_gpio) {
			pr_debug("%s: enable Bottom spkr amp gpio\n", __func__);
			gpio_direction_output(bottom_spk_pamp_gpio, 1);
	} else if (spk_amp_gpio == top_spk_pamp_gpio) {
			pr_debug("%s: enable Top spkr amp gpio\n", __func__);
			gpio_direction_output(top_spk_pamp_gpio, 1);
	} else {
		pr_err("%s: ERROR : Invalid External Speaker Ampl GPIO."
			" gpio = %u\n", __func__, spk_amp_gpio);
		return;
	}
}

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

			msm8960_enable_ext_spk_amp_gpio(bottom_spk_pamp_gpio);
			pr_debug("%s: slepping 4 ms after turning on external "
				" Bottom Speaker Ampl\n", __func__);
			usleep_range(4000, 4000);
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

			/*msm8960_enable_ext_spk_amp_gpio(top_spk_pamp_gpio);
			pr_info("%s: sleeping 4 ms after turning on "
				" external Top Speaker Ampl\n", __func__);
			usleep_range(4000, 4000);*/

			schedule_delayed_work(
			&ext_amp_dwork.dwork,
			msecs_to_jiffies(50));
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

		gpio_direction_output(bottom_spk_pamp_gpio, 0);
		msm8960_ext_bottom_spk_pamp = 0;

		pr_debug("%s: sleeping 4 ms after turning off external Bottom"
			" Speaker Ampl\n", __func__);

		/*usleep_range(4000, 4000);*/
		msleep(30);

	} else if (spk & (TOP_SPK_AMP_POS | TOP_SPK_AMP_NEG)) {

		if (!msm8960_ext_top_spk_pamp)
			return;

		gpio_direction_output(top_spk_pamp_gpio, 0);
		msm8960_ext_top_spk_pamp = 0;

		pr_debug("%s: sleeping 4 ms after turning off external Top"
			" Spkaker Ampl\n", __func__);

		/*usleep_range(4000, 4000);*/

		msleep(30);
	} else  {

		pr_err("%s: ERROR : Invalid Ext Spk Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void msm8960_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

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
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	}
	return 0;
}

#if defined(CONFIG_MACH_JAGUAR)
static int msm8960_ear_switch_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("%s: %s Ear Switch gpio\n", __func__,
		SND_SOC_DAPM_EVENT_ON(event) ? "Enable" : "Disable");

	gpio_direction_output(ear_switch_gpio, SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}
#endif

#if defined(CONFIG_MACH_ESPRESSO10_ATT) || defined(CONFIG_MACH_ESPRESSO10_VZW) \
	|| defined(CONFIG_MACH_ESPRESSO10_SPR) || defined(CONFIG_MACH_KONA)
static int msm8960_lineout_switch_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("%s: %s Lineout Switch gpio\n", __func__,
		SND_SOC_DAPM_EVENT_ON(event) ? "Enable" : "Disable");

	gpio_set_value(GPIO_CRADLE_SW_EN, SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}
#endif
static struct mutex cdc_mclk_mutex;

static int msm8960_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
		bool dapm)
{
	int r = 0;
	pr_debug("%s: enable = %d\n", __func__, enable);
	mutex_lock(&cdc_mclk_mutex);
	if (enable) {
		clk_users++;
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users == 1) {
			codec_clk = clk_get(NULL, "i2s_spkr_osr_clk");
			if (codec_clk) {
				clk_set_rate(codec_clk, TABLA_EXT_CLK_RATE);
				clk_enable(codec_clk);
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
			pr_debug("%s: clk_users = %d\n", __func__,
			 clk_users);
			if (clk_users == 0) {
				pr_debug("%s: disabling MCLK. clk_users = %d\n",
						__func__, clk_users);
				tabla_mclk_enable(codec, 0, dapm);
				clk_disable(codec_clk);
				clk_put(codec_clk);
			}
		} else {
				pr_err("%s: Error releasing Tabla MCLK\n",
				 __func__);
				r = -EINVAL;
		}
	}
	mutex_unlock(&cdc_mclk_mutex);
	return r;
}

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

static const struct snd_soc_dapm_widget msm8960_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8960_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Ext Spk Bottom Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_SPK("Ext Spk Top Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Top Neg", msm8960_spkramp_event),
#if defined(CONFIG_MACH_ESPRESSO10_ATT) || defined(CONFIG_MACH_ESPRESSO10_VZW) \
	|| defined(CONFIG_MACH_ESPRESSO10_SPR) || defined(CONFIG_MACH_KONA)
	SND_SOC_DAPM_SPK("LINEOUT Switch", msm8960_lineout_switch_event),
#endif
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

#if defined(CONFIG_MACH_JAGUAR)
static const struct snd_soc_dapm_widget msm8960_dapm_widgets_rev16[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8960_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Ext Spk Bottom Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_SPK("Ext Spk Top Pos", msm8960_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Top Neg", msm8960_spkramp_event),

	SND_SOC_DAPM_SPK("Ear Switch", msm8960_ear_switch_event),

	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
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
#endif

static const struct snd_soc_dapm_route common_audio_map[] = {

	{"EAR_RX_BIAS", NULL, "MCLK"},
	{"RX_BIAS", NULL, "MCLK"},
	{"LDO_H", NULL, "MCLK"},

	/* Speaker path */

	{"Ext Spk Top Pos", NULL, "LINEOUT2"},
	{"Ext Spk Top Neg", NULL, "LINEOUT4"},
#if defined(CONFIG_MACH_ESPRESSO10_ATT) || defined(CONFIG_MACH_ESPRESSO10_VZW) \
	|| defined(CONFIG_MACH_ESPRESSO10_SPR) || defined(CONFIG_MACH_KONA)
	/*
	 * ESPRESSO10_ATT has analog switch for
	 * reducing the pop noise on dock path
	 */
	{"LINEOUT Switch", NULL, "LINEOUT5"},
#endif
	/* Microphone path */
	/**
	 *Samsung uses AMIC4 for Handset sub Mic
	 *AMIC4 uses external MIC BIAS1
	 */
#if defined(CONFIG_MACH_KONA)
	{"AMIC4", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Sub Mic"},

	{"AMIC2", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Headset Mic"},
#else
	{"AMIC4", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Handset Mic"},

	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},
#endif

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

#if defined(CONFIG_MACH_JAGUAR)
static const struct snd_soc_dapm_route common_audio_map_rev16[] = {

	{"EAR_RX_BIAS", NULL, "MCLK"},
	{"RX_BIAS", NULL, "MCLK"},
	{"LDO_H", NULL, "MCLK"},

	/* Speaker path */

	{"Ext Spk Top Pos", NULL, "LINEOUT2"},
	{"Ext Spk Top Neg", NULL, "LINEOUT4"},

	/* Microphone path */
	/**
	 *Samsung uses AMIC4 for Handset sub Mic
	 *AMIC4 uses external MIC BIAS1
	 */
	{"AMIC4", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Handset Mic"},

	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},

	/**
	 * AMIC3 and AMIC4 inputs are connected to ANC microphones
	 * These mics are biased differently on CDP and FLUID
	 * routing entries below are based on bias arrangement
	 * on FLUID.
	 */
	{ "AMIC3", NULL, "MIC BIAS1 External" },

	{"HEADPHONE", NULL, "LDO_H"},
	{"Ear Switch", NULL, "HEADPHONE"},

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
#endif

static const char * const spk_function[] = {"Off", "On"};
static const char * const slim0_rx_ch_text[] = {"One", "Two"};
static const char * const slim0_tx_ch_text[] = {"One", "Two", "Three", "Four"};
static const char * const slim0_sample_rate_text[] = {
			"Eight", "Sixteen", "FourtyEight"};

static const struct soc_enum msm8960_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim0_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(3, slim0_sample_rate_text),
};

static const char * const btsco_rate_text[] = {"8000", "16000"};
static const struct soc_enum msm8960_btsco_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
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

static int msm8960_slim_0_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	pr_debug("%s: msm8960_slim_0_sample_rate  = %d\n", __func__,
			msm8960_slim_0_sample_rate);

	ucontrol->value.integer.value[0] = msm8960_slim_0_sample_rate;


	return 0;
}



static int msm8960_slim_0_sample_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8960_slim_0_sample_rate = 8000;
		break;

	case 1:
		msm8960_slim_0_sample_rate = 16000;
		break;

	case 2:
		msm8960_slim_0_sample_rate = 48000;
		break;

	default:
		msm8960_slim_0_sample_rate = 48000;
		break;
	}

	pr_debug("%s : msm8960_slim_0_sample_rate  =%d\n",
			__func__, msm8960_slim_0_sample_rate);
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
	case 0:
		msm8960_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	case 1:
		msm8960_btsco_rate = BTSCO_RATE_16KHZ;
		break;
	default:
		msm8960_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	}
	pr_debug("%s: msm8960_btsco_rate = %d\n", __func__,
					msm8960_btsco_rate);
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
};

static const struct snd_kcontrol_new tabla_msm8960_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm8960_enum[0], msm8960_get_spk,
		msm8960_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm8960_enum[1],
		msm8960_slim_0_rx_ch_get, msm8960_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm8960_enum[2],
		msm8960_slim_0_tx_ch_get, msm8960_slim_0_tx_ch_put),
	SOC_ENUM_EXT("SLIM_0 SampleRate", msm8960_enum[3],
		msm8960_slim_0_sample_rate_get, msm8960_slim_0_sample_rate_put),

};

static const struct snd_kcontrol_new int_btsco_rate_mixer_controls[] = {
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm8960_btsco_enum[0],
		msm8960_btsco_rate_get, msm8960_btsco_rate_put),
};

static int msm8960_btsco_init(struct snd_soc_pcm_runtime *rtd)
{
	int err = 0;
	struct snd_soc_platform *platform = rtd->platform;

	err = snd_soc_add_platform_controls(platform,
			int_btsco_rate_mixer_controls,
		ARRAY_SIZE(int_btsco_rate_mixer_controls));
	if (err < 0)
		return err;
	return 0;
}

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
	S(v_hs_max, 1700);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, TABLA_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal);
	btn_low = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_LOW);
	btn_high = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -85;
	btn_high[0] = 85;
	btn_low[1] = 86;
	btn_high[1] = 205;
	btn_low[2] = 206;
	btn_high[2] = 500;
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
	err = snd_soc_add_controls(codec, tabla_msm8960_i2s_controls,
				ARRAY_SIZE(tabla_msm8960_i2s_controls));
	if (err < 0) {
		pr_err("returning loc 1 err = %d\n", err);
		return err;
	}

	snd_soc_dapm_new_controls(dapm, msm8960_dapm_widgets,
			ARRAY_SIZE(msm8960_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, common_audio_map,
		ARRAY_SIZE(common_audio_map));
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
			SND_JACK_BTN_0, &volumeup_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Volumedown Jack",
			SND_JACK_BTN_0, &volumedown_jack);
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

			tabla_hs_detect(codec, &mbhc_cfg);


	return 0;
}

static int msm8960_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	struct pm_gpio jack_gpio_cfg = {
		.direction = PM_GPIO_DIR_IN,
		.pull = PM_GPIO_PULL_UP_1P5,
		.function = PM_GPIO_FUNC_NORMAL,
		.vin_sel = 2,
		.inv_int_pol = 0,
	};

	pr_debug("%s()\n", __func__);

	if (machine_is_msm8960_liquid()) {
		top_spk_pamp_gpio = (PM8921_GPIO_PM_TO_SYS
						(PMIC_GPIO_VPS_EN));
		bottom_spk_pamp_gpio = (PM8921_GPIO_PM_TO_SYS
						(PMIC_GPIO_SPK_EN));
	}

	rtd->pmdown_time = 0;

	err = snd_soc_add_controls(codec, tabla_msm8960_controls,
				ARRAY_SIZE(tabla_msm8960_controls));
	if (err < 0)
		return err;

#if defined(CONFIG_MACH_JAGUAR)
	if (system_rev >= BOARD_REV16)
		snd_soc_dapm_new_controls(dapm, msm8960_dapm_widgets_rev16,
			ARRAY_SIZE(msm8960_dapm_widgets_rev16));
	else
#endif
		snd_soc_dapm_new_controls(dapm, msm8960_dapm_widgets,
			ARRAY_SIZE(msm8960_dapm_widgets));

#if defined(CONFIG_MACH_JAGUAR)
	if (system_rev >= BOARD_REV16)
		snd_soc_dapm_add_routes(dapm, common_audio_map_rev16,
			ARRAY_SIZE(common_audio_map_rev16));
	else
#endif
		snd_soc_dapm_add_routes(dapm, common_audio_map,
			ARRAY_SIZE(common_audio_map));

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
		       TABLA_JACK_BUTTON_MASK, &button_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Volumeup Jack",
			SND_JACK_BTN_0, &volumeup_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Volumedown Jack",
			SND_JACK_BTN_0, &volumedown_jack);
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

	if ((machine_is_jaguar() && system_rev < BOARD_REV16) ||
		(machine_is_APEXQ() && system_rev < BOARD_REV02) ||
		(machine_is_ESPRESSO_VZW() && system_rev < BOARD_REV03) ||
		(!machine_is_jaguar() && !machine_is_ESPRESSO_VZW()
			&& !machine_is_ESPRESSO10_VZW()
			&& !machine_is_ESPRESSO10_SPR()
			&& !machine_is_ESPRESSO10_ATT()
			&& !machine_is_ESPRESSO_SPR()
			&& !machine_is_KONA()
		&& !machine_is_APEXQ())) {
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
		tabla_hs_detect(codec, &mbhc_cfg);

	}

	return 0;
}

static struct snd_soc_dsp_link lpa_fe_media = {
	.playback = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static struct snd_soc_dsp_link fe_media = {
	.playback = true,
	.capture = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static struct snd_soc_dsp_link slimbus0_hl_media = {
	.playback = true,
	.capture = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static struct snd_soc_dsp_link int_fm_hl_media = {
	.playback = true,
	.capture = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

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

	rate->min = rate->max = msm8960_slim_0_sample_rate;

	channels->min = channels->max = msm8960_slim_0_rx_ch;

	pr_info("%s channels %u () sample rate =%d\n",
			__func__, channels->min, rate->min);
	return 0;
}

static int msm8960_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8960_slim_0_sample_rate;

	channels->min = channels->max = msm8960_slim_0_tx_ch;

	pr_info("%s channels %u () sample rate =%d\n",
			__func__, channels->min, rate->min);
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
#if defined (CONFIG_SND_SOC_MSM_QDSP6_HDMI_AUDIO)
static int msm8960_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
			channels->min, channels->max);

	rate->min = rate->max = 48000;

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

	/* PCM only supports mono output with 8khz sample rate */
	rate->min = rate->max = 8000;
	channels->min = channels->max = 1;

	return 0;
}
int msm8960_aux_pcm_get_gpios(void)
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

int msm8960_aux_pcm_free_gpios(void)
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
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rx_bit_clk) {
			clk_disable(rx_bit_clk);
			clk_put(rx_bit_clk);
			clk_disable(codec_clk);
			clk_put(codec_clk);
			rx_bit_clk = NULL;
		}
		msm8960_cdc_i2s_rx_free_gpios();
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (tx_bit_clk) {
			clk_disable(tx_bit_clk);
			clk_put(tx_bit_clk);
			rx_bit_clk = NULL;
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

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		configure_i2s_rx_gpio();
		codec_clk = clk_get(NULL, "i2s_spkr_osr_clk");
		if (codec_clk) {
			clk_set_rate(codec_clk, I2S_MCLK_RATE);
			clk_enable(codec_clk);
		}
		rx_bit_clk = clk_get(NULL, "i2s_spkr_bit_clk");
		if (IS_ERR(rx_bit_clk)) {
			pr_err("Failed to get i2s_spkr_bit_clk\n");
			clk_disable(codec_clk);
			clk_put(codec_clk);
			return PTR_ERR(rx_bit_clk);
		}
		clk_set_rate(rx_bit_clk, 8);
		ret = clk_enable(rx_bit_clk);
		if (ret != 0) {
			pr_err("Unable to enable i2s_spkr_bit_clk\n");
			clk_disable(codec_clk);
			clk_put(codec_clk);
			clk_put(rx_bit_clk);
			return ret;
		}
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("set format for cpu dai failed\n");
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("set format for codec dai failed\n");

	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		configure_i2s_tx_gpio();

		tx_osr_clk = clk_get(NULL, "i2s_mic_osr_clk");
		if (IS_ERR(tx_osr_clk)) {
			pr_debug("Failed to get i2s_mic_osr_clk\n");
			return PTR_ERR(rx_osr_clk);
		}
		/* Master clock OSR 256 */
		clk_set_rate(tx_osr_clk, 1536000);
		ret = clk_enable(tx_osr_clk);
		if (ret != 0) {
			pr_debug("Unable to enable i2s_mic_osr_clk\n");
			clk_put(tx_osr_clk);
			return ret;
		}
		tx_bit_clk = clk_get(NULL, "i2s_mic_bit_clk");
		if (IS_ERR(tx_bit_clk)) {
			pr_debug("Failed to get i2s_mic_bit_clk\n");
			clk_disable(tx_osr_clk);
			clk_put(tx_osr_clk);
			return PTR_ERR(tx_bit_clk);
		}
		clk_set_rate(tx_bit_clk, 8);
		ret = clk_enable(tx_bit_clk);
		if (ret != 0) {
			pr_debug("Unable to enable i2s_mic_bit_clk\n");
			clk_put(tx_bit_clk);
			clk_disable(tx_osr_clk);
			clk_put(tx_osr_clk);
			return ret;
		}
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);

		if (ret < 0)
			pr_err("set format for cpu dai failed\n");

		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("set format for codec dai failed\n");
	}
	return ret;
}

static int msm8960_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

#if !defined(CONFIG_MACH_ESPRESSO10_ATT) && !defined(CONFIG_MACH_APEXQ) && !defined (CONFIG_MACH_ESPRESSO10_SPR) \
	&& !defined (CONFIG_MACH_ESPRESSO10_VZW) && !defined (CONFIG_MACH_ESPRESSO_VZW)
static int msm8960_auxpcm_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s(): substream = %s\n", __func__, substream->name);
	ret = msm8960_aux_pcm_get_gpios();
	if (ret < 0) {
		pr_err("%s: Aux PCM GPIO request failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}
#endif

#if !defined(CONFIG_MACH_ESPRESSO10_ATT) && !defined(CONFIG_MACH_APEXQ) && !defined (CONFIG_MACH_ESPRESSO10_SPR) \
	&& !defined (CONFIG_MACH_ESPRESSO10_VZW) && !defined (CONFIG_MACH_ESPRESSO_VZW)
static void msm8960_auxpcm_shutdown(struct snd_pcm_substream *substream)
{

	pr_debug("%s(): substream = %s\n", __func__, substream->name);
	msm8960_aux_pcm_free_gpios();
}
#endif

static void msm8960_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
}

static struct snd_soc_ops msm8960_be_ops = {
	.startup = msm8960_startup,
	.shutdown = msm8960_shutdown,
};

static struct snd_soc_ops msm8960_i2s_be_ops = {
	.startup = msm8960_i2s_startup,
	.shutdown = msm8960_i2s_shutdown,
	.hw_params = msm8660_i2s_hw_params,
};

#if !defined(CONFIG_MACH_ESPRESSO10_ATT) && !defined(CONFIG_MACH_APEXQ) && !defined (CONFIG_MACH_ESPRESSO10_SPR) \
	&& !defined (CONFIG_MACH_ESPRESSO10_VZW) && !defined (CONFIG_MACH_ESPRESSO_VZW)
static struct snd_soc_ops msm8960_auxpcm_be_ops = {
	.startup = msm8960_auxpcm_startup,
	.shutdown = msm8960_auxpcm_shutdown,
};
#endif

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

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8960_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8960 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8960 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name	= "MultiMedia2",
		.platform_name  = "msm-multi-ch-pcm-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "MSM8960 LPA",
		.stream_name = "LPA",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
		.dsp_link = &lpa_fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PMC purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name	= "SLIMBUS0_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dsp_link = &slimbus0_hl_media,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* .be_id = do not care */
	},
	{
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dsp_link = &int_fm_hl_media,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* .be_id = do not care */
	},
	{
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
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
		.dsp_link = &lpa_fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	/* Backend BT/FM DAI Links */
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.init = &msm8960_btsco_init,
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = msm8960_btsco_be_hw_params_fixup,
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
		.no_codec = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm8960_hdmi_be_hw_params_fixup,
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
		.no_codec = 1,
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_codec = 1,
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
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
		.no_codec = 1,
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
		.no_codec = 1,
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
		.no_codec = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
	},
};

struct snd_soc_card snd_soc_card_msm8960 = {
	.name		= "msm8960-snd-card",
	.dai_link	= msm8960_dai,
	.num_links	= ARRAY_SIZE(msm8960_dai),
};

static struct platform_device *msm8960_snd_device;

#ifndef PMIC_GPIO_USEURO_SWITCH
#define PMIC_GPIO_USEURO_SWITCH 35
#endif

static int msm8960_configure_headset_mic_gpios(void)
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
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		return ret;
		} else
		gpio_direction_output(PM8921_GPIO_PM_TO_SYS(23), 0);

	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH),
		"US_EURO_SWITCH");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH));
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		return ret;
	}
	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH),
		&param);
	if (ret) {
		pr_err("%s: Failed to configure gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH));
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		gpio_free(PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH));
		return ret;
		} else
		gpio_direction_output(
			PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH), 0);

	ret = gpio_request(bottom_spk_pamp_gpio, "BOTTOM_SPK_AMP");
	if (ret) {
		pr_err("%s: Error requesting BOTTOM SPK AMP GPIO %u\n",
			__func__, bottom_spk_pamp_gpio);
	}
	ret = pm8xxx_gpio_config(bottom_spk_pamp_gpio, &param);
	if (ret) {
		pr_err("%s: Failed to configure Bottom Spk Ampl"
			" gpio %u\n", __func__, bottom_spk_pamp_gpio);
		 gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		 gpio_free(PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH));
		 gpio_free(bottom_spk_pamp_gpio);
		return ret;
	} else
		gpio_direction_output(bottom_spk_pamp_gpio, 0);

#if defined(CONFIG_MACH_APEXQ)
if (system_rev < BOARD_REV01)
	top_spk_pamp_gpio  = PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_SPK_EN);
else
	top_spk_pamp_gpio  = PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_SPK_EN_BOARD_01);
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
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		gpio_free(PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH));
		gpio_free(bottom_spk_pamp_gpio);
		gpio_free(top_spk_pamp_gpio);
		return ret;
	} else
		gpio_direction_output(top_spk_pamp_gpio, 0);

#if defined(CONFIG_MACH_JAGUAR)
	if (system_rev >= BOARD_REV16) {
		ret = gpio_request(ear_switch_gpio, "EAR_OUT_SEL");
		if (ret) {
			pr_err("%s: Failed to request gpio %d\n", __func__,
				ear_switch_gpio);
			return ret;
		}

		ret = pm8xxx_gpio_config(ear_switch_gpio, &param);
		if (ret) {
			pr_err("%s: Failed to configure gpio %d\n", __func__,
				ear_switch_gpio);
			gpio_free(ear_switch_gpio);
			return ret;
		} else
			gpio_direction_output(ear_switch_gpio, 0);
	}
#endif

#if defined(CONFIG_MACH_ESPRESSO10_ATT) || defined(CONFIG_MACH_ESPRESSO10_VZW) \
	|| defined(CONFIG_MACH_ESPRESSO10_SPR) || defined(CONFIG_MACH_KONA)
	ret = gpio_request(GPIO_CRADLE_SW_EN, "CRADLE_SW_EN");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
			GPIO_CRADLE_SW_EN);
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		gpio_free(PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH));
		gpio_free(bottom_spk_pamp_gpio);
		gpio_free(top_spk_pamp_gpio);
		return ret;
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_CRADLE_SW_EN, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
#endif
	return 0;
}
static void msm8960_free_headset_mic_gpios(void)
{
	if (msm8960_headset_gpios_configured) {
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		gpio_free(PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_USEURO_SWITCH));
		gpio_free(top_spk_pamp_gpio);
		gpio_free(bottom_spk_pamp_gpio);
#if defined(CONFIG_MACH_ESPRESSO10_ATT) || defined(CONFIG_MACH_ESPRESSO10_VZW) \
	|| defined(CONFIG_MACH_ESPRESSO10_SPR) || defined(CONFIG_MACH_KONA)
		gpio_free(GPIO_CRADLE_SW_EN);
#endif
	}
}

static int __init msm8960_audio_init(void)
{
	int ret;
	msm8960_dai_list = kzalloc(sizeof(msm8960_dai) +
			2 * sizeof(struct snd_soc_dai_link), GFP_KERNEL);
	if (tabla_get_intf_type() == TABLA_INTERFACE_TYPE_SLIMBUS) {
		memcpy(msm8960_dai_list, msm8960_dai, sizeof(msm8960_dai));
		memcpy(&msm8960_dai_list[ARRAY_SIZE(msm8960_dai)],
			msm8960_slimbus_be_dai, sizeof(msm8960_slimbus_be_dai));
		snd_soc_card_msm8960.dai_link = msm8960_dai_list;
		snd_soc_card_msm8960.num_links = ARRAY_SIZE(msm8960_dai) +
					ARRAY_SIZE(msm8960_slimbus_be_dai);
	} else if (tabla_get_intf_type() == TABLA_INTERFACE_TYPE_I2C) {
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

	if (msm8960_configure_headset_mic_gpios()) {
		pr_err("%s Fail to configure headset mic gpios\n", __func__);
		msm8960_headset_gpios_configured = 0;
	} else
		msm8960_headset_gpios_configured = 1;

	mutex_init(&cdc_mclk_mutex);
	
	INIT_DELAYED_WORK(&ext_amp_dwork.dwork,
			external_speaker_amp_work);
	return ret;

}
module_init(msm8960_audio_init);

static void __exit msm8960_audio_exit(void)
{
	msm8960_free_headset_mic_gpios();
	kfree(msm8960_dai_list);
	platform_device_unregister(msm8960_snd_device);
	kfree(mbhc_cfg.calibration);
	mutex_destroy(&cdc_mclk_mutex);
}
module_exit(msm8960_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MSM8960");
MODULE_LICENSE("GPL v2");
