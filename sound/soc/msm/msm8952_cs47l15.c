/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <soc/qcom/socinfo.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "msm-audio-pinctrl.h"
#include "../codecs/msm8x16-wcd.h"
#include "../codecs/wsa881x-analog.h"
#include <linux/regulator/consumer.h>

#ifdef CONFIG_SEC_MPP_SHARE
#include <linux/sec_mux_sel.h>
#endif /* CONFIG_SEC_MPP_SHARE */
#ifdef CONFIG_SAMSUNG_JACK
#include <linux/sec_jack.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/qpnp/pin.h>
#endif /* CONFIG_SAMSUNG_JACK */
#define DRV_NAME "msm8952-asoc-cs47l15"
#if defined(CONFIG_SND_SOC_CS47L15)
#include <linux/mfd/madera/core.h>
#include <linux/extcon/extcon-madera.h>
#include "../codecs/madera.h"
#include <linux/debugfs.h>
#include <sound/tlv.h>
#endif

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_192KHZ    192000

#define PRI_MI2S_ID	(1 << 0)
#define SEC_MI2S_ID	(1 << 1)
#define TER_MI2S_ID	(1 << 2)
#define QUAT_MI2S_ID	(1 << 3)
#define QUIN_MI2S_ID	(1 << 4)

#define DEFAULT_MCLK_RATE 9600000

#define WCD_MBHC_DEF_RLOADS 5
#define MAX_WSA_CODEC_NAME_LENGTH 80
#define MSM_DT_MAX_PROP_SIZE 80

#if defined(CONFIG_SND_SOC_CS47L15)
/* Used for debugging and test automation */
static u32 voice_trigger_count;

static struct snd_soc_card  *msm8952_cs47l15_card;
static DECLARE_TLV_DB_SCALE(digital_tlv, -6400,50,0);

/* Debugfs value overrides, default to 0 */
static unsigned int forced_mclk1;
static unsigned int forced_sysclk;
static unsigned int forced_dspclk;

#define MCLK_RATE	24576000
struct clk *cs47l15_ext_clk;
unsigned long mclk_freq = 24000000;
bool mclk_enable = false;
int sample_rate_i2s_cs47l15 = 48000;

/* CLEARWATER has fixed post dividers for the FLL output */
#define FLL_RATE	(12 * MCLK_RATE)
#define SYSCLK_RATE	(FLL_RATE / 3)
#define DSPCLK_RATE	(FLL_RATE / 2)

struct msm8952_cs47l15_notifier_context {	
	struct notifier_block nb;	
	struct device *dev;
};

struct gain_table {
	int min;           /* Minimum impedance */
	int max;           /* Maximum impedance */
	unsigned int gain; /* Register value to set for this measurement */
};

static struct impedance_table {
	struct gain_table hp_gain_table[7];
	char imp_region[4];	/* impedance region */
} imp_table = {
	.hp_gain_table = {
		{    0,      13, 0 },
		{   14,      42, 8 },
		{   43,     100, 14 },
		{  101,     200, 20 },
		{  201,     450, 22 },
		{  451,    1000, 22 },
		{ 1001, INT_MAX, 18 },
	},
};

#endif

enum btsco_rates {
	RATE_8KHZ_ID,
	RATE_16KHZ_ID,
};

static int msm8952_auxpcm_rate = 8000;
static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_btsco_ch = 1;
static int msm_ter_mi2s_tx_ch = 1;
static int msm_pri_mi2s_rx_ch = 1;
static int msm_proxy_rx_ch = 2;
static int msm_vi_feed_tx_ch = 2;
static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mi2s_rx_bits_per_sample = 16;
static int mi2s_rx_sample_rate = SAMPLING_RATE_48KHZ;

static atomic_t quat_mi2s_clk_ref;
static atomic_t quin_mi2s_clk_ref;
static atomic_t auxpcm_mi2s_clk_ref;

static int msm8952_enable_dig_cdc_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);
#ifdef CONFIG_SND_SOC_WCD_MBHC
static bool msm8952_swap_gnd_mic(struct snd_soc_codec *codec);
#endif /* #ifdef CONFIG_SND_SOC_WCD_MBHC */
static int msm8952_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);
static int msm8952_wsa_switch_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);

#ifdef CONFIG_SND_SOC_WCD_MBHC
/*
 * Android L spec
 * Need to report LINEIN
 * if R/L channel impedance is larger than 5K ohm
 */
static struct wcd_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.detect_extn_cable = true,
	.mono_stero_detection = false,
	.swap_gnd_mic = NULL,
	.hs_ext_micbias = false,
	.key_code[0] = KEY_MEDIA,
	.key_code[1] = KEY_VOICECOMMAND,
	.key_code[2] = KEY_VOLUMEUP,
	.key_code[3] = KEY_VOLUMEDOWN,
	.key_code[4] = 0,
	.key_code[5] = 0,
	.key_code[6] = 0,
	.key_code[7] = 0,
	.linein_th = 5000,
};
#endif /* CONFIG_SND_SOC_WCD_MBHC */

#ifdef CONFIG_SAMSUNG_JACK
static struct snd_soc_jack hs_jack;
static struct mutex jack_mutex;
#endif /* CONFIG_SAMSUNG_JACK */

static struct afe_clk_cfg mi2s_rx_clk_v1 = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

static struct afe_clk_cfg mi2s_tx_clk_v1 = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

static struct afe_clk_cfg wsa_ana_clk_v1 = {
	AFE_API_VERSION_I2S_CONFIG,
	0,
	Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK2_VALID,
	0,
};

static struct afe_clk_set mi2s_tx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set mi2s_rx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set wsa_ana_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_MCLK_1,
	Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE"};
static const char *const mi2s_ch_text[] = {"One", "Two"};
static const char *const loopback_mclk_text[] = {"DISABLE", "ENABLE"};
static const char *const btsco_rate_text[] = {"BTSCO_RATE_8KHZ",
	"BTSCO_RATE_16KHZ"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *mi2s_rx_sample_rate_text[] = {"KHZ_48",
					"KHZ_96", "KHZ_192"};

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);

		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}

static const struct snd_soc_dapm_widget msm8952_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("MCLK", -1, SND_SOC_NOPM, 0, 0,
	msm8952_mclk_event, SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_SUPPLY("VDD_WSA_SWITCH", SND_SOC_NOPM, 0, 0,
	msm8952_wsa_switch_event,
	SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
#if defined(CONFIG_SND_SOC_CS47L15)
	SND_SOC_DAPM_MIC("HeadsetMic", NULL),
	SND_SOC_DAPM_MIC("MainMic", NULL),
	SND_SOC_DAPM_MIC("SubMic", NULL),
#endif
};
static const struct snd_soc_dapm_widget cs47l15_widgets[] = {	
		SND_SOC_DAPM_MIC("DMIC", NULL),	
		SND_SOC_DAPM_MIC("HEADSETMIC", NULL),
};

static const struct snd_soc_dapm_route msm8952_cs47l15_routes[] = {	
		{ "DMIC", NULL, "MICBIAS1C" },	
		{ "HEADSETMIC", NULL, "MICBIAS1A" },	
		{ "HEADSETMIC", NULL, "MICBIAS1B" },	
		{ "IN1AL", NULL, "DMIC" },	
		{ "IN1BR", NULL, "HEADSETMIC" },
};

int is_ext_spk_gpio_support(struct platform_device *pdev,
			struct msm8916_asoc_mach_data *pdata)
{
	const char *spk_ext_pa = "qcom,msm-spk-ext-pa";

	pr_debug("%s:Enter\n", __func__);

	pdata->spk_ext_pa_gpio = of_get_named_gpio(pdev->dev.of_node,
				spk_ext_pa, 0);

	if (pdata->spk_ext_pa_gpio < 0) {
		dev_dbg(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, spk_ext_pa);
	} else {
		if (!gpio_is_valid(pdata->spk_ext_pa_gpio)) {
			pr_err("%s: Invalid external speaker gpio: %d",
				__func__, pdata->spk_ext_pa_gpio);
			return -EINVAL;
		}
	}
	return 0;
}

#ifdef CONFIG_SND_SOC_WCD_MBHC
/* Validate whether US EU switch is present or not */
int is_us_eu_switch_gpio_support(struct platform_device *pdev,
		struct msm8916_asoc_mach_data *pdata)
{
	int ret;

	pr_debug("%s\n", __func__);

	/* check if US-EU GPIO is supported */
	pdata->us_euro_gpio = of_get_named_gpio(pdev->dev.of_node,
					"qcom,cdc-us-euro-gpios", 0);
	if (pdata->us_euro_gpio < 0) {
		dev_dbg(&pdev->dev,
			"property %s in node %s not found %d\n",
			"qcom,cdc-us-euro-gpios", pdev->dev.of_node->full_name,
			pdata->us_euro_gpio);
	} else {
		if (!gpio_is_valid(pdata->us_euro_gpio)) {
			pr_err("%s: Invalid gpio: %d", __func__,
					pdata->us_euro_gpio);
			return -EINVAL;
		}
		ret = msm_get_gpioset_index(CLIENT_WCD_INT,
						"us_eu_gpio");
		if (ret < 0) {
			pr_err("%s: gpio set name does not exist: %s",
						__func__, "us_eu_gpio");
			return ret;
		}
		mbhc_cfg.swap_gnd_mic = msm8952_swap_gnd_mic;
	}
	return 0;
}
#endif /* CONFIG_SND_SOC_WCD_MBHC */

static int msm_proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__,
						msm_proxy_rx_ch);
	ucontrol->value.integer.value[0] = msm_proxy_rx_ch - 1;
	return 0;
}

static int msm_proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_proxy_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__,
						msm_proxy_rx_ch);
	return 0;
}

static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8952_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static int msm_pri_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: Num of channels = %d Sample rate = %d\n", __func__,
			msm_pri_mi2s_rx_ch, mi2s_rx_sample_rate);
	rate->min = rate->max = mi2s_rx_sample_rate;
	channels->min = channels->max = msm_pri_mi2s_rx_ch;

	return 0;
}

static int msm_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s(), channel:%d\n", __func__, msm_ter_mi2s_tx_ch);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			SNDRV_PCM_FORMAT_S16_LE);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_ter_mi2s_tx_ch;

	return 0;
}

static int msm_senary_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	return 0;
}


static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	 if (192000==params_rate(params)&&mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
	   	rate->min = rate->max = 192000;
	else  
		rate->min = rate->max = 48000;//params_rate(params);

	pr_debug("%s() sample rate = %d  mi2s_rx_bit_format = %d \n", __func__, params_rate(params), mi2s_rx_bit_format);
	channels->min = channels->max = 2;

	return 0;
}

static int msm_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm_btsco_rate;
	channels->min = channels->max = msm_btsco_ch;

	return 0;
}

static int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: msm_proxy_rx_ch =%d\n", __func__, msm_proxy_rx_ch);

	if (channels->max < 2)
		channels->min = channels->max = 2;
	channels->min = channels->max = msm_proxy_rx_ch;
	rate->min = rate->max = 48000;
	return 0;
}

static int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 48000;
	return 0;
}

static int msm_mi2s_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       mi2s_rx_bit_format);
	else
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       SNDRV_PCM_FORMAT_S16_LE);


	sample_rate_i2s_cs47l15 = params_rate(params);

	pr_debug("%s(): rate = %d, format = %d\n", __func__,sample_rate_i2s_cs47l15,mi2s_rx_bit_format);

	return 0;
}

static int msm8952_get_clk_id(int port_id)
{
	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
		return Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT;
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
		return Q6AFE_LPASS_CLK_ID_SEC_MI2S_IBIT;
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT;
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT;
	case AFE_PORT_ID_QUINARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT;
	case AFE_PORT_ID_SENARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_SEN_MI2S_IBIT;
	default:
		pr_err("%s: invalid port_id: 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
}

static int msm8952_get_port_id(int be_id)
{
	switch (be_id) {
	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		return AFE_PORT_ID_PRIMARY_MI2S_RX;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		return AFE_PORT_ID_SECONDARY_MI2S_RX;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		return AFE_PORT_ID_TERTIARY_MI2S_TX;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		return AFE_PORT_ID_QUATERNARY_MI2S_RX;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		return AFE_PORT_ID_QUATERNARY_MI2S_TX;
	case MSM_BACKEND_DAI_QUINARY_MI2S_RX:
		return AFE_PORT_ID_QUINARY_MI2S_RX;
	case MSM_BACKEND_DAI_QUINARY_MI2S_TX:
		return AFE_PORT_ID_QUINARY_MI2S_TX;
	case MSM_BACKEND_DAI_SENARY_MI2S_TX:
		return AFE_PORT_ID_SENARY_MI2S_TX;
	default:
		pr_err("%s: Invalid be_id: %d\n", __func__, be_id);
		return -EINVAL;
	}
}

static uint32_t get_mi2s_rx_clk_val(int port_id)
{
	uint32_t clk_val;

	/*
	 *  Derive clock value based on configuration of Primary MI2S rx port,
	 *  as this port supports dynamic configuration for hifi audio
	 */
	if (port_id == AFE_PORT_ID_PRIMARY_MI2S_RX) {
		clk_val = (mi2s_rx_sample_rate * mi2s_rx_bits_per_sample * 2);
	} else {
		if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
			clk_val =   Q6AFE_LPASS_IBIT_CLK_12_P288_MHZ;//Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
		else
			clk_val = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
	}
	pr_debug("%s: MI2S Rx bit clock value: 0x%0x\n", __func__, clk_val);
	return clk_val;
}

#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
static int msm_q6_enable_mi2s_clocks(bool enable, int port_id)
{
	union afe_port_config port_config;
	int rc = 0;

	pr_info("set msm_q6_enable_mi2s_clocks enable=%d\n", enable);

	if (enable) {
		port_config.i2s.channel_mode = AFE_PORT_I2S_SD1;
		port_config.i2s.mono_stereo = MSM_AFE_CH_STEREO;
		port_config.i2s.data_format = 0;
		port_config.i2s.bit_width = 16;
		port_config.i2s.reserved = 0;
		port_config.i2s.i2s_cfg_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
		port_config.i2s.sample_rate = 48000;
		port_config.i2s.ws_src = 1;

		rc = afe_port_start(port_id, &port_config, 48000);

		if (IS_ERR_VALUE(rc)) {
			pr_err("fail to open AFE port\n");
			return -EINVAL;
		}
	} else {
		rc = afe_close(port_id);
		if (IS_ERR_VALUE(rc)) {
			pr_err("fail to close AFE port\n");
			return -EINVAL;
		}
	}
	return rc;
}
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */

static int msm_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int port_id = 0;

	port_id = msm8952_get_port_id(rtd->dai_link->be_id);
	//pr_debug("msm_mi2s_sclk_ctl   port ID %x, enable :%d  %d\n", port_id, enable, (int) q6core_get_avs_version());

	if (port_id < 0) {
		pr_err("%s: Invalid port_id\n", __func__);
		return -EINVAL;
	}
	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_rx_clk_v1.clk_val1 =
						get_mi2s_rx_clk_val(port_id);
				ret = afe_set_lpass_clock(port_id,
							&mi2s_rx_clk_v1);
			} else {
				mi2s_rx_clk.enable = enable;
				mi2s_rx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				mi2s_rx_clk.clk_freq_in_hz =
						get_mi2s_rx_clk_val(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_rx_clk);
			}
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			 if (port_id == AFE_PORT_ID_QUATERNARY_MI2S_RX)
			    msm_q6_enable_mi2s_clocks(true, port_id);
#endif
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_tx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_tx_clk_v1);
			} else {
				mi2s_tx_clk.enable = enable;
				mi2s_tx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				mi2s_tx_clk.clk_freq_in_hz =
						Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_tx_clk);
			}
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (port_id == AFE_PORT_ID_QUATERNARY_MI2S_TX)
			msm_q6_enable_mi2s_clocks(true, port_id);
#endif
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock_v2 failed\n", __func__);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (port_id == AFE_PORT_ID_QUATERNARY_MI2S_RX)
				msm_q6_enable_mi2s_clocks(false, port_id);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_rx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_rx_clk_v1);
			} else {
				mi2s_rx_clk.enable = enable;
				mi2s_rx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_rx_clk);
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (port_id == AFE_PORT_ID_QUATERNARY_MI2S_TX)
				msm_q6_enable_mi2s_clocks(false, port_id);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_tx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_tx_clk_v1);
			} else {
				mi2s_tx_clk.enable = enable;
				mi2s_tx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_tx_clk);
			}
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock_v2 failed\n", __func__);
	}
	return ret;
}

static int msm8952_enable_dig_cdc_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: enable %d mclk ref counter %d\n",
		   __func__, enable,
		   atomic_read(&pdata->mclk_rsc_ref));
	if (enable) {
		if (!atomic_read(&pdata->mclk_rsc_ref)) {
			cancel_delayed_work_sync(
					&pdata->disable_mclk_work);
			mutex_lock(&pdata->cdc_mclk_mutex);
			if (atomic_read(&pdata->mclk_enabled) == false) {
				if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
					pdata->digital_cdc_clk.clk_val =
							pdata->mclk_freq;
					ret = afe_set_digital_codec_core_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_clk);
				} else {
					pdata->digital_cdc_core_clk.enable = 1;
					ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_core_clk);
				}
				if (ret < 0) {
					pr_err("%s: failed to enable CCLK\n",
							__func__);
					mutex_unlock(&pdata->cdc_mclk_mutex);
					return ret;
				}
				pr_debug("enabled digital codec core clk\n");
				atomic_set(&pdata->mclk_enabled, true);
			}
			mutex_unlock(&pdata->cdc_mclk_mutex);
		}
		atomic_inc(&pdata->mclk_rsc_ref);
	} else {
		cancel_delayed_work_sync(&pdata->disable_mclk_work);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == true) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				pdata->digital_cdc_clk.clk_val = 0;
				ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			} else {
				pdata->digital_cdc_core_clk.enable = 0;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_core_clk);
			}
			if (ret < 0)
				pr_err("%s: failed to disable CCLK\n",
						__func__);
			atomic_set(&pdata->mclk_enabled, false);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	return ret;
}

static int msm_btsco_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_btsco_rate  = %d", __func__, msm_btsco_rate);
	ucontrol->value.integer.value[0] = msm_btsco_rate;
	return 0;
}

static int msm_btsco_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case RATE_8KHZ_ID:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	case RATE_16KHZ_ID:
		msm_btsco_rate = BTSCO_RATE_16KHZ;
		break;
	default:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	}

	pr_debug("%s: msm_btsco_rate = %d\n", __func__, msm_btsco_rate);
	return 0;
}

static int mi2s_rx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (mi2s_rx_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: mi2s_rx_bit_format = %d, ucontrol value = %ld\n",
			__func__, mi2s_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int mi2s_rx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		mi2s_rx_bits_per_sample = 32;
		break;
	case 0:
	default:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		mi2s_rx_bits_per_sample = 16;
		break;
	}
	return 0;
}

static int loopback_mclk_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int loopback_mclk_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: mclk_rsc_ref %d enable %ld\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref),
			ucontrol->value.integer.value[0]);
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		ret = msm_gpioset_activate(CLIENT_WCD_INT, "pri_i2s");
		if (ret) {
			pr_err("%s: failed to enable the pri gpios: %d\n",
					__func__, ret);
			break;
		}
		mutex_lock(&pdata->cdc_mclk_mutex);
		if ((!atomic_read(&pdata->mclk_rsc_ref)) &&
				(!atomic_read(&pdata->mclk_enabled))) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				pdata->digital_cdc_clk.clk_val =
						pdata->mclk_freq;
				ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			} else {
				pdata->digital_cdc_core_clk.enable = 1;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_core_clk);
			}
			if (ret < 0) {
				pr_err("%s: failed to enable the MCLK: %d\n",
						__func__, ret);
				mutex_unlock(&pdata->cdc_mclk_mutex);
				ret = msm_gpioset_suspend(CLIENT_WCD_INT,
								"pri_i2s");
				if (ret)
					pr_err("%s: failed to disable the pri gpios: %d\n",
							__func__, ret);
				break;
			}
			atomic_set(&pdata->mclk_enabled, true);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
		atomic_inc(&pdata->mclk_rsc_ref);
		msm8x16_wcd_mclk_enable(codec, 1, true);
		break;
	case 0:
		if (atomic_read(&pdata->mclk_rsc_ref) <= 0)
			break;
		msm8x16_wcd_mclk_enable(codec, 0, true);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if ((!atomic_dec_return(&pdata->mclk_rsc_ref)) &&
				(atomic_read(&pdata->mclk_enabled))) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				pdata->digital_cdc_clk.clk_val = 0;
				ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			} else {
				pdata->digital_cdc_core_clk.enable = 0;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_core_clk);
			}
			if (ret < 0) {
				pr_err("%s: failed to disable the CCLK: %d\n",
						__func__, ret);
				mutex_unlock(&pdata->cdc_mclk_mutex);
				break;
			}
			atomic_set(&pdata->mclk_enabled, false);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "pri_i2s");
		if (ret)
			pr_err("%s: failed to disable the pri gpios: %d\n",
					__func__, ret);
		break;
	default:
		pr_err("%s: Unexpected input value\n", __func__);
		break;
	}
	return ret;
}

static int msm_pri_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_mi2s_rx_ch  = %d\n", __func__,
		 msm_pri_mi2s_rx_ch);
	ucontrol->value.integer.value[0] = msm_pri_mi2s_rx_ch - 1;
	return 0;
}

static int msm_pri_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_pri_mi2s_rx_ch = %d\n", __func__, msm_pri_mi2s_rx_ch);
	return 1;
}

static int mi2s_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (mi2s_rx_sample_rate) {
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: sample_rate_val = %d\n", __func__,
		 sample_rate_val);

	return 0;
}

static int mi2s_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		mi2s_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 2:
		mi2s_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 0:
	default:
		mi2s_rx_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mi2s_rx_sample_rate = %d\n", __func__,
		 mi2s_rx_sample_rate);
	return 0;
}

static int msm_ter_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_ter_mi2s_tx_ch  = %d\n", __func__,
		 msm_ter_mi2s_tx_ch);
	ucontrol->value.integer.value[0] = msm_ter_mi2s_tx_ch - 1;
	return 0;
}

static int msm_ter_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_ter_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_ter_mi2s_tx_ch = %d\n", __func__, msm_ter_mi2s_tx_ch);
	return 1;
}

static int msm_vi_feed_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = (msm_vi_feed_tx_ch/2 - 1);
	pr_debug("%s: msm_vi_feed_tx_ch = %ld\n", __func__,
				ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_vi_feed_tx_ch_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	msm_vi_feed_tx_ch =
		roundup_pow_of_two(ucontrol->value.integer.value[0] + 2);

	pr_debug("%s: msm_vi_feed_tx_ch = %d\n", __func__, msm_vi_feed_tx_ch);
	return 1;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_bit_format_text),
				rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mi2s_ch_text),
				mi2s_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(loopback_mclk_text),
				loopback_mclk_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(btsco_rate_text),
				btsco_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(proxy_rx_ch_text),
				proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vi_feed_ch_text),
				vi_feed_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mi2s_rx_sample_rate_text),
				mi2s_rx_sample_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("MI2S_RX Format", msm_snd_enum[0],
			mi2s_rx_bit_format_get, mi2s_rx_bit_format_put),
	SOC_ENUM_EXT("MI2S_TX Channels", msm_snd_enum[1],
			msm_ter_mi2s_tx_ch_get, msm_ter_mi2s_tx_ch_put),
	SOC_ENUM_EXT("MI2S_RX Channels", msm_snd_enum[1],
			msm_pri_mi2s_rx_ch_get, msm_pri_mi2s_rx_ch_put),
	SOC_ENUM_EXT("Loopback MCLK", msm_snd_enum[2],
			loopback_mclk_get, loopback_mclk_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_snd_enum[3],
		     msm_btsco_rate_get, msm_btsco_rate_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[4],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", msm_snd_enum[5],
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
	SOC_ENUM_EXT("MI2S_RX SampleRate", msm_snd_enum[6],
			mi2s_rx_sample_rate_get, mi2s_rx_sample_rate_put),
};

static int msm8952_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	int ret = 0;

	pdata = snd_soc_card_get_drvdata(w->codec->component.card);
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		pr_debug("%s: mclk_res_ref = %d\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref));
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "pri_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "pri_i2s");
			return ret;
		}
		if (atomic_read(&pdata->mclk_rsc_ref) == 0) {
			pr_debug("%s: disabling MCLK\n", __func__);
			/* disable the codec mclk config*/
			msm8x16_wcd_mclk_enable(w->codec, 0, true);
			msm8952_enable_dig_cdc_clk(w->codec, 0, true);
		}
		break;
	default:
		pr_err("%s: invalid DAPM event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static int msm8952_wsa_switch_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct on_demand_supply *supply;

	pdata = snd_soc_card_get_drvdata(w->codec->component.card);
	supply = &pdata->wsa_switch_supply;
	if (!supply->supply) {
		dev_err(w->codec->component.card->dev, "%s: no wsa switch supply",
			__func__);
		return ret;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (atomic_inc_return(&supply->ref) == 1)
			ret = regulator_enable(supply->supply);
		if (ret)
			dev_err(w->codec->component.card->dev,
				"%s: Failed to enable wsa switch supply\n",
				__func__);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_read(&supply->ref) == 0) {
			dev_dbg(w->codec->component.card->dev,
				"%s: wsa switch supply has been disabled.\n",
				__func__);
			return ret;
		}
		if (atomic_dec_return(&supply->ref) == 0)
			ret = regulator_disable(supply->supply);
			if (ret)
				dev_err(w->codec->component.card->dev,
					"%s: Failed to disable wsa switch supply\n",
					__func__);
		break;
	default:
		break;
	}

	return ret;
}

static int msm8952_enable_wsa_mclk(struct snd_soc_card *card, bool enable)
{
	int ret = 0;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	mutex_lock(&pdata->wsa_mclk_mutex);
	if (enable) {
		if (!atomic_read(&pdata->wsa_mclk_rsc_ref)) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				wsa_ana_clk_v1.clk_val1 =
						Q6AFE_LPASS_OSR_CLK_9_P600_MHZ;
				ret = afe_set_lpass_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&wsa_ana_clk_v1);
			} else {
				wsa_ana_clk.enable = enable;
				ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&wsa_ana_clk);
			}
			if (ret < 0) {
				pr_err("%s: failed to enable mclk %d\n",
					__func__, ret);
				goto done;
			}
		}
		atomic_inc(&pdata->wsa_mclk_rsc_ref);
	} else {
		if (!atomic_read(&pdata->wsa_mclk_rsc_ref))
			goto done;
		if (!atomic_dec_return(&pdata->wsa_mclk_rsc_ref)) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				wsa_ana_clk_v1.clk_val1 =
						Q6AFE_LPASS_OSR_CLK_DISABLE;
				ret = afe_set_lpass_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&wsa_ana_clk_v1);
			} else {
				wsa_ana_clk.enable = enable;
				ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&wsa_ana_clk);
			}
			if (ret < 0) {
				pr_err("%s: failed to disable mclk %d\n",
					__func__, ret);
				goto done;
			}
		}
	}

done:
	mutex_unlock(&pdata->wsa_mclk_mutex);
	return ret;
}

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	/*
	 * configure the slave select to
	 * invalid state for internal codec
	 */
	if (pdata->vaddr_gpio_mux_spkr_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_spkr_ctl);
		val = val | 0x00010000;
		iowrite32(val, pdata->vaddr_gpio_mux_spkr_ctl);
	}

	if (pdata->vaddr_gpio_mux_mic_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
		val = val | 0x00200000;
		iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
	}

	ret = msm_mi2s_sclk_ctl(substream, true);
	if (ret < 0) {
		pr_err("%s: failed to enable sclk %d\n",
				__func__, ret);
		return ret;
	}
	if (card->aux_dev && substream->stream ==
			SNDRV_PCM_STREAM_PLAYBACK) {
		ret = msm8952_enable_wsa_mclk(card, true);
		if (ret < 0) {
			pr_err("%s: failed to enable mclk for wsa %d\n",
				__func__, ret);
			return ret;
		}
	}
	ret =  msm8952_enable_dig_cdc_clk(codec, 1, true);
	if (ret < 0) {
		pr_err("failed to enable mclk\n");
		return ret;
	}
	/* Enable the codec mclk config */
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "pri_i2s");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be activated %sd",
				__func__, "pri_i2s");
		return ret;
	}
	msm8x16_wcd_mclk_enable(codec, 1, true);
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed; ret=%d\n", __func__, ret);

	return ret;
}
static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);

	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed; ret=%d\n", __func__,
				ret);
	if (card->aux_dev && substream->stream ==
			SNDRV_PCM_STREAM_PLAYBACK) {
		ret = msm8952_enable_wsa_mclk(card, false);
		if (ret < 0) {
			pr_err("%s: failed to disable mclk for wsa %d\n",
				__func__, ret);
		}
	}
	if (atomic_read(&pdata->mclk_rsc_ref) > 0) {
		atomic_dec(&pdata->mclk_rsc_ref);
		pr_debug("%s: decrementing mclk_res_ref %d\n",
				__func__, atomic_read(&pdata->mclk_rsc_ref));
	}
}

static int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s\n",
			__func__, substream->name);

	/* mux config to route the AUX MI2S */
	if (pdata->vaddr_gpio_mux_mic_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
		val = val | 0x2;
		iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
	}
	if (pdata->vaddr_gpio_mux_pcm_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_pcm_ctl);
		val = val | 0x1;
		iowrite32(val, pdata->vaddr_gpio_mux_pcm_ctl);
	}
	msm8952_enable_dig_cdc_clk(codec, 1, true);
	atomic_inc(&auxpcm_mi2s_clk_ref);

	/* enable the gpio's used for the external AUXPCM interface */
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0)
		pr_err("%s(): configure gpios failed = %s\n",
				__func__, "quat_i2s");
	return ret;
}

static void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s\n",
			__func__, substream->name);
	if (atomic_read(&pdata->mclk_rsc_ref) > 0) {
		atomic_dec(&pdata->mclk_rsc_ref);
		pr_debug("%s: decrementing mclk_res_ref %d\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref));
	}
	if (atomic_read(&auxpcm_mi2s_clk_ref) > 0)
		atomic_dec(&auxpcm_mi2s_clk_ref);
	if ((atomic_read(&auxpcm_mi2s_clk_ref) == 0) &&
		(atomic_read(&pdata->mclk_rsc_ref) == 0)) {
		msm8952_enable_dig_cdc_clk(codec, 0, true);
	}
	ret = msm_gpioset_suspend(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0)
		pr_err("%s(): configure gpios failed = %s\n",
				__func__, "quat_i2s");
}

static int msm_sec_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm8916_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_info("%s: Secondary Mi2s does not support capture\n",
					__func__);
		return 0;
	}
	if ((pdata->ext_pa & SEC_MI2S_ID) == SEC_MI2S_ID) {
		if (pdata->vaddr_gpio_mux_spkr_ctl) {
			val = ioread32(pdata->vaddr_gpio_mux_spkr_ctl);
			val = val | 0x0004835c;
			iowrite32(val, pdata->vaddr_gpio_mux_spkr_ctl);
		}
		ret = msm_mi2s_sclk_ctl(substream, true);
		if (ret < 0) {
			pr_err("failed to enable sclk\n");
			return ret;
		}
		pr_debug("%s(): SEC I2S gpios turned on  = %s\n", __func__,
				"sec_i2s");
		ret = msm_gpioset_activate(CLIENT_WCD_INT, "sec_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be activated %sd",
						__func__, "sec_i2s");
			goto err;
		}
	} else {
			pr_err("%s: error codec type\n", __func__);
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		pr_err("%s: set fmt cpu dai failed\n", __func__);
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "sec_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated %sd",
						__func__, "sec_i2s");
			goto err;
		}
	}
	return ret;
err:
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_sec_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if ((pdata->ext_pa & SEC_MI2S_ID) == SEC_MI2S_ID) {
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "sec_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated: %sd",
					__func__, "sec_i2s");
			return;
		}
		ret = msm_mi2s_sclk_ctl(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed\n", __func__);
	}
}

static int msm_quat_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm8916_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if (pdata->vaddr_gpio_mux_mic_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
		val = val | 0x02020002;
		iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
	}
	ret = msm_mi2s_sclk_ctl(substream, true);
	if (ret < 0) {
		pr_err("failed to enable sclk\n");
		return ret;
	}
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0) {
		pr_err("failed to enable codec gpios\n");
		goto err;
	}
	if (atomic_inc_return(&quat_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;
err:
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_quat_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed\n", __func__);
	if (atomic_read(&quat_mi2s_clk_ref) > 0)
		atomic_dec(&quat_mi2s_clk_ref);
	ret = msm_gpioset_suspend(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "quat_i2s");
		return;
	}
}

static int msm_quin_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm8916_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if (pdata->vaddr_gpio_mux_quin_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_quin_ctl);
		val = val | 0x00000001;
		iowrite32(val, pdata->vaddr_gpio_mux_quin_ctl);
	} else {
		return -EINVAL;
	}
	ret = msm_mi2s_sclk_ctl(substream, true);
	if (ret < 0) {
		pr_err("failed to enable sclk\n");
		return ret;
	}
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "quin_i2s");
	if (ret < 0) {
		pr_err("failed to enable codec gpios\n");
		goto err;
	}
	if (atomic_inc_return(&quin_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;
err:
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_quin_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed\n", __func__);
	if (atomic_read(&quin_mi2s_clk_ref) > 0)
		atomic_dec(&quin_mi2s_clk_ref);
	ret = msm_gpioset_suspend(CLIENT_WCD_INT, "quin_i2s");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "quin_i2s");
		return;
	}
}

#ifdef CONFIG_SND_SOC_WCD_MBHC
static void *def_msm8952_wcd_mbhc_cal(void)
{
	void *msm8952_wcd_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;

	msm8952_wcd_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!msm8952_wcd_cal)
		return NULL;

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(msm8952_wcd_cal)->X) = (Y))
	S(v_hs_max, 1500);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(msm8952_wcd_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S


	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(msm8952_wcd_cal);
	btn_low = btn_cfg->_v_btn_low;
	btn_high = ((void *)&btn_cfg->_v_btn_low) +
		(sizeof(btn_cfg->_v_btn_low[0]) * btn_cfg->num_btn);

	/*
	 * In SW we are maintaining two sets of threshold register
	 * one for current source and another for Micbias.
	 * all btn_low corresponds to threshold for current source
	 * all bt_high corresponds to threshold for Micbias
	 * Below thresholds are based on following resistances
	 * 0-70    == Button 0
	 * 110-180 == Button 1
	 * 210-290 == Button 2
	 * 360-680 == Button 3
	 */
	btn_low[0] = 75;
	btn_high[0] = 75;
	btn_low[1] = 150;
	btn_high[1] = 150;
	btn_low[2] = 225;
	btn_high[2] = 225;
	btn_low[3] = 450;
	btn_high[3] = 450;
	btn_low[4] = 500;
	btn_high[4] = 500;

	return msm8952_wcd_cal;
}
#endif /* CONFIG_SND_SOC_WCD_MBHC */

#if defined(CONFIG_SND_SOC_CS47L15)
void msm8952_madera_hpdet_cb(unsigned int meas)
{
	struct snd_soc_card *card = msm8952_cs47l15_card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);	
	int jack_det;
	int i, num_hp_gain_table;

	if (meas == MADERA_HP_Z_OPEN) {
		jack_det = 0;
	} else {
		jack_det = 1;
	}

	dev_info(card->dev, "%s(%d) meas(%d)\n", __func__, jack_det, meas);

	num_hp_gain_table = (int) ARRAY_SIZE(imp_table.hp_gain_table);
	for (i = 0; i < num_hp_gain_table; i++) {
		if (meas < imp_table.hp_gain_table[i].min
				|| meas > imp_table.hp_gain_table[i].max)
			continue;

		dev_info(card->dev, "SET GAIN %d step for %d ohms\n",
			 imp_table.hp_gain_table[i].gain, meas);
		pdata->hp_impedance_step = imp_table.hp_gain_table[i].gain;
	}
}
void msm8952_update_impedance_table(struct device_node *np)
{
	int len = ARRAY_SIZE(imp_table.hp_gain_table);
	u32 data[len * 3];
	int i, ret;
	char imp_str[14] = "imp_table";


	if (strlen(imp_table.imp_region) == 3) {
		strcat(imp_str, "_");
		strcat(imp_str, imp_table.imp_region);
	}

	if (of_find_property(np, imp_str, NULL))
	{
		ret = of_property_read_u32_array(np, imp_str, data, (len * 3));
	}
	else
	{
		ret = of_property_read_u32_array(np, "imp_table", data,
								(len * 3));
	}

	if (!ret) {

		for (i = 0; i < len; i++) {
			imp_table.hp_gain_table[i].min = data[i * 3];
			imp_table.hp_gain_table[i].max = data[(i * 3) + 1];
			imp_table.hp_gain_table[i].gain = data[(i * 3) + 2];
		}
	}
}

static int __init get_impedance_region(char *str)
{
	/* Read model region */
	
	strncat(imp_table.imp_region, str,
				sizeof(imp_table.imp_region) - 1);

	return 0;
}
early_param("region1", get_impedance_region);

static int madera_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	//struct snd_soc_card *card = codec->card;
	struct snd_soc_card *card=msm8952_cs47l15_card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	unsigned int val, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);
	val += pdata->hp_impedance_step;
	dev_info(card->dev,
			 "SET GAIN %d according to impedance, moved %d step\n",
			 val, pdata->hp_impedance_step);

	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;

	err = snd_soc_update_bits(codec, reg, val_mask, val);
	if (err < 0)
		return err;

	return err;
}



static int msm8952_cs47l15_notifier_cb(struct notifier_block *nb,				  
unsigned long event, void *data)
{	
	const struct madera_hpdet_notify_data *hp_inf = data;	
	const struct madera_micdet_notify_data *md_inf = data;	
	const struct madera_voice_trigger_info *tr_inf = data;	
	
	const struct msm8952_cs47l15_notifier_context *ctx =		
		container_of(nb, struct msm8952_cs47l15_notifier_context, nb);	

	switch (event) {	
		case MADERA_NOTIFY_VOICE_TRIGGER:		
			dev_info(ctx->dev, "Voice Triggered (core_num %d)\n",			
				tr_inf->core_num);
			
			++voice_trigger_count;		
			break;	
		case MADERA_NOTIFY_HPDET:		
				dev_info(ctx->dev, "HPDET val=%d.%02d ohms\n",			 
					hp_inf->impedance_x100 / 100,			 
					hp_inf->impedance_x100 % 100);		
				break;	
		case MADERA_NOTIFY_MICDET:		
			dev_info(ctx->dev, "MICDET present=%c val=%d.%02d ohms\n",			
				md_inf->present ? 'Y' : 'N',			 
				md_inf->impedance_x100 / 100,			 
				md_inf->impedance_x100 % 100);		
			break;	
		default:		
			dev_info(ctx->dev, "notifier event=0x%lx data=0x%p\n",			
				event, data);		
			break;	
			}	

	return 0;
}

static struct msm8952_cs47l15_notifier_context lochnagar_notifier = 
	{	.nb = {		
	.notifier_call = msm8952_cs47l15_notifier_cb,	
		},
	};


static int msm8952_cs47l15_set_refclk(struct snd_soc_card *card)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	struct snd_soc_codec *codec = codec_dai->codec;	
	unsigned int fll_src, fin, fout;	
	int ret;	

	if (forced_mclk1) 
	{		
		/* use 32kHz input to avoid overclocking the FLL when		 
		* forcing a specific MCLK frequency into the codec		 
		* FLL calculations		 */	
		
		fll_src = MADERA_FLL_SRC_MCLK2;		
		fin = forced_mclk1;	
	} 
	else {		
		fll_src = MADERA_FLL_SRC_MCLK1;		
		fin = mclk_freq; //MCLK_RATE;	
	}	

	if (forced_sysclk)		
		fout = forced_sysclk;	
	else		
		fout = SYSCLK_RATE;	
	
	dev_dbg(card->dev, "Setting FLL1 to fin=%u Hz fout=%u Hz\n",		
		fin, fout);	
	
	/* This will give us an FLL output which is	 
	* post divided for SYSCLK/DSPCLK (/3, /2)	 
	* so we could in theory use either, but use	 
	* SYSCLK for consistency with other madera	 
	* devices. */	
	
	ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_REFCLK, fll_src, fin, fout);	
	if (ret)		
		dev_err(card->dev, "Failed to start FLL1: %d\n", ret);	
	
	return ret;
}

static int msm8952_cs47l15_stop_refclk(struct snd_soc_card *card)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	struct snd_soc_codec *codec = codec_dai->codec;	
	int ret;	

	ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_REFCLK, 0, 0, 0);	
	if (ret)		
		dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);	

	return ret;
}
static int msm8952_cs47l15_set_sysclk(struct snd_soc_card *card)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	struct snd_soc_codec *codec = codec_dai->codec;	
	unsigned int freq;	int ret;	

	if (forced_sysclk)		
		freq = forced_sysclk;	
	else		
		freq = SYSCLK_RATE;
	
	dev_dbg(card->dev, "Setting SYSCLK to %u Hz\n", freq);	
	ret = snd_soc_codec_set_sysclk(codec, MADERA_CLK_SYSCLK,				       
		MADERA_CLK_SRC_FLL1,				       
		freq,				       
		SND_SOC_CLOCK_IN);	
	if (ret)		
		dev_err(card->dev, "Failed to set SYSCLK: %d\n", ret);	
	
	return ret;
}

static int msm8952_cs47l15_set_dspclk(struct snd_soc_card *card)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	struct snd_soc_codec *codec = codec_dai->codec;	
	unsigned int freq;	
	int ret;	

	if (forced_dspclk)		
		freq = forced_dspclk;	
	else		
		freq = DSPCLK_RATE;	

	dev_dbg(card->dev, "Setting DSPCLK to %u Hz\n", freq);	

	ret = snd_soc_codec_set_sysclk(codec, MADERA_CLK_DSPCLK,				      
		MADERA_CLK_SRC_FLL1,				       
		freq,				      
		SND_SOC_CLOCK_IN);	
	if (ret)		
		dev_err(card->dev, "Failed to set DSPCLK: %d\n", ret);	

	return ret;
}

static int msm_madera_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	//struct snd_soc_dai *aif1_dai = rtd->codec_dai;
	struct snd_soc_dapm_context *codec_dapm = &codec->dapm; 
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dapm_context *dapm = &card->dapm;
   	// int ret;
    
	//dev_info(rtd->card->dev, "%s: setup %s clocks\n", __func__, codec->name);

	/*
	ret = snd_soc_dai_set_sysclk(aif1_dai, MADERA_CLK_SYSCLK, 0, 0);
	if (ret != 0) {
		dev_err(rtd->card->dev, "Failed to set AIF1 clock: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_SYNCCLK,
				    MADERA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(rtd->card->dev, "Failed to clear refclk: %d\n", ret);
		return ret;
	}
      */
	snd_soc_dapm_ignore_suspend(dapm, "HeadsetMic");
	snd_soc_dapm_ignore_suspend(dapm, "MainMic");
	snd_soc_dapm_ignore_suspend(dapm, "SubMic");

	snd_soc_dapm_sync(dapm);

	snd_soc_dapm_ignore_suspend(codec_dapm,"AIF1 Playback");
	snd_soc_dapm_ignore_suspend(codec_dapm,"AIF1 Capture");
#if 0
	snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUT1L");
	snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUT1R");
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1BL");
	snd_soc_dapm_ignore_suspend(codec_dapm,"AIF1TX1");
	snd_soc_dapm_ignore_suspend(codec_dapm,"AIF1RX2");
	snd_soc_dapm_ignore_suspend(codec_dapm,"AIF1RX1");
	snd_soc_dapm_ignore_suspend(codec_dapm,"OUT1L");
	snd_soc_dapm_ignore_suspend(codec_dapm,"OUT1R");
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1L PGA");
#endif
	//snd_soc_dapm_ignore_suspend(codec_dapm,"Tone Generator 1");
	//snd_soc_dapm_ignore_suspend(codec_dapm,"SYSCLK");
	//snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUT1L");
	//snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUT1R");
	snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUTL");
	snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUTR");
	snd_soc_dapm_ignore_suspend(codec_dapm,"EPOUTN");
	snd_soc_dapm_ignore_suspend(codec_dapm,"EPOUTP"); 
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1BL");
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1AL");
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1AR");
	
	snd_soc_dapm_sync(codec_dapm);

	return 0;
}

static int msm8952_cs47l15_set_pll(struct snd_soc_card *card)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	struct snd_soc_codec *codec = codec_dai->codec;	
	unsigned int sample_rate, bits_per_sample;
	int sysclk;
	int ret;

	sysclk = 98304000;
	sample_rate = sample_rate_i2s_cs47l15;

	if(192000 == sample_rate)
		bits_per_sample = 64;
	else
		bits_per_sample = 32;

	dev_dbg(card->dev, "Setting PLL freq to %u Hz\n", sysclk);	
	
	ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_SYNCCLK,
					MADERA_FLL_SRC_AIF1BCLK, sample_rate * bits_per_sample,sysclk);
	
	if (ret != 0) {
		dev_err(card->dev,"Failed to set FLL1 to %dHz: %d\n", sysclk, ret);
		return ret;
	}
	
	return ret;
}

static int msm8952_cs47l15_stop_pll(struct snd_soc_card *card)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	struct snd_soc_codec *codec = codec_dai->codec;	
	int ret;


	dev_dbg(card->dev, "stop PLL freq!\n");	
	
	ret = snd_soc_codec_set_pll(codec, MADERA_CLK_SYSCLK,
					MADERA_FLL_SRC_NONE, 0,0);
	
	if (ret != 0) {
		dev_err(card->dev,"Failed to stop FLL1!\n");
		return ret;
	}
	
	return ret;
}


#ifdef CONFIG_DEBUG_FS
static int msm8952_cs47l15_force_fll1_enable_write(void *data, u64 val)
{	
	struct snd_soc_card *card = data;	
	int ret;	

	if (val == 0)		
		ret = msm8952_cs47l15_stop_refclk(card);	
	else		
		ret = msm8952_cs47l15_set_refclk(card);	
	
	return ret;

}
DEFINE_SIMPLE_ATTRIBUTE(lochnagar_force_fll1_enable_fops, NULL,			
	msm8952_cs47l15_force_fll1_enable_write, "%llu\n");

static void msm8952_cs47l15_init_debugfs(struct snd_soc_card *card)
{	
	struct dentry *root;	

	if (!card->debugfs_card_root) 
	{		
		dev_warn(card->dev, "No card debugfs root\n");		
		return;	
	}	

	root = debugfs_create_dir("test-automation", card->debugfs_card_root);	
	  if (!root)
	  {		
	  	dev_warn(card->dev, "Failed to create debugfs dir\n");		
		return;	
	}	

	  debugfs_create_u32("voice_trigger_count", S_IRUGO, root,			   
	  	&voice_trigger_count);	
	  
	  debugfs_create_u32("forced_mclk1", S_IRUGO | S_IWUGO, root,			   
	  	&forced_mclk1);	
	  debugfs_create_u32("forced_sysclk", S_IRUGO | S_IWUGO, root,			  
	  	&forced_sysclk);	
	  debugfs_create_u32("forced_dspclk", S_IRUGO | S_IWUGO, root,			   
	  	&forced_dspclk);	
	  
	  debugfs_create_file("force_fll1_enable", S_IWUSR | S_IWGRP, root, card,			   
	  	&lochnagar_force_fll1_enable_fops);
	  }
#else
static void msm8952_cs47l15_init_debugfs(struct snd_soc_card *card)
{
}
#endif

static const struct snd_kcontrol_new msm_cs47l15_codec_controls[] = {
	SOC_SINGLE_EXT_TLV("HPOUT1L Impedance Volume",
		MADERA_DAC_DIGITAL_VOLUME_1L,
		MADERA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, madera_put_impedance_volsw,
		digital_tlv),

	SOC_SINGLE_EXT_TLV("HPOUT1R Impedance Volume",
		MADERA_DAC_DIGITAL_VOLUME_1R,
		MADERA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, madera_put_impedance_volsw,
		digital_tlv),
};

static int msm8952_cs47l15_late_probe(struct snd_soc_card *card)
{	
	struct snd_soc_codec *codec = card->rtd[56].codec;	
	struct snd_soc_dai *aif1_dai = card->rtd[56].codec_dai;	
	int ret;	

	msm8952_cs47l15_card=card;
	madera_set_hpdet_cb(codec, msm8952_madera_hpdet_cb);
	
	ret = snd_soc_dai_set_sysclk(aif1_dai, MADERA_CLK_SYSCLK, 0, 0);	
	 if (ret != 0) 
		{		
		      dev_err(aif1_dai->dev, "Failed to set AIF1 clock: %d\n", ret);		
			return ret;	
		}	
	 
	msm8952_cs47l15_init_debugfs(card);	
	
	madera_register_notifier(codec, &lochnagar_notifier.nb);	
	
	/* Enable trigger pin for trigger-only SC Control */	
	snd_soc_dapm_enable_pin(&codec->dapm, "DSP Trigger Out");	
	
	/* Enable DRC2 pin for SC Control that uses DRC2 as a SAD */	
	snd_soc_dapm_enable_pin(&codec->dapm, "DRC2 Signal Activity");	

	ret = snd_soc_add_codec_controls(codec, msm_cs47l15_codec_controls,
					ARRAY_SIZE(msm_cs47l15_codec_controls));
	
	return 0;
}

static void msm8952_cs47l15_set_mclk(int enable)
{
	int ret;

	printk("%s enable %d\n", __func__, enable);

	if(mclk_enable == enable)	
	{
		printk("%s  msm8952_cs47l15_set_mclk already enable! igonor operate enable %d\n", __func__, enable);
		return;
	}
	
	if (enable) {
		ret = clk_set_rate(cs47l15_ext_clk, 
			(unsigned long)mclk_freq);
		if (ret < 0) {
			printk("%s: Can't set ext-mclk rate %dHz (%d)\n",
			__func__, (int)mclk_freq, ret);
			return;
		}

		ret = clk_prepare_enable(cs47l15_ext_clk);
		if (ret < 0) {
			printk( "%s: Can't enable ext-mclk %d\n",
			__func__, ret);
			return;
		}
	} else {
		clk_disable_unprepare(cs47l15_ext_clk);
	}

	mclk_enable = enable;
	
	return;
}


static int msm8952_cs47l15_set_bias_level(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm,				  
	enum snd_soc_bias_level level)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	int ret;

	dev_dbg(card->dev, "%s: set bias level=%d\n", __func__,level);
	
	if (dapm->dev != codec_dai->dev)		
		return 0;	
	
	switch (level) {	
       case SND_SOC_BIAS_PREPARE:		
			if (dapm->bias_level != SND_SOC_BIAS_STANDBY)			
				break;	
			
			msm8952_cs47l15_set_mclk(1);
			
			ret = msm8952_cs47l15_set_sysclk(card);		
			if (ret)			
				return ret;	
			
			ret = msm8952_cs47l15_set_dspclk(card);		
			if (ret)			
				return ret;		
			
			ret = msm8952_cs47l15_set_refclk(card);		
			if (ret)			
				return ret;

			ret = msm8952_cs47l15_set_pll(card);
			if(ret)
				return ret;

			break;	

		default:		
				break;	
	}	
	
	return 0;
}

static int msm8952_cs47l15_set_bias_level_post(struct snd_soc_card *card,	
					struct snd_soc_dapm_context *dapm,					 
					enum snd_soc_bias_level level)
{	
	struct snd_soc_dai *codec_dai = card->rtd[56].codec_dai;	
	int ret;

	dev_dbg(card->dev, "%s: set bias post level=%d\n", __func__,level);
	
	if (dapm->dev != codec_dai->dev)		
		return 0;	
	
	switch (level) 
		{	
		 case SND_SOC_BIAS_STANDBY:		
			ret = msm8952_cs47l15_stop_refclk(card);
			if (ret)
				return ret;

			ret = msm8952_cs47l15_stop_pll(card);		
			if (ret)			
				return ret;

			msm8952_cs47l15_set_mclk(0);
			break;	
		
		default:		
				break;	
		}
	
	dapm->bias_level = level;	
	return 0;
	
}
#endif
static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
#ifdef CONFIG_SND_SOC_WCD_MBHC
	int ret = -ENOMEM;
#endif /* CONFIG_SND_SOC_WCD_MBHC */

	pr_debug("%s(),dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	snd_soc_add_codec_controls(codec, msm_snd_controls,
			ARRAY_SIZE(msm_snd_controls));

	snd_soc_dapm_new_controls(dapm, msm8952_dapm_widgets,
			ARRAY_SIZE(msm8952_dapm_widgets));

	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Secondary Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");

	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "SPK_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_SPK OUT");

	snd_soc_dapm_sync(dapm);
	
#ifdef CONFIG_SAMSUNG_JACK
	hs_jack.codec = codec;
#endif /* CONFIG_SAMSUNG_JACK */

#ifdef CONFIG_SND_SOC_WCD_MBHC
	mbhc_cfg.calibration = def_msm8952_wcd_mbhc_cal();
	if (mbhc_cfg.calibration) {
		ret = msm8x16_wcd_hs_detect(codec, &mbhc_cfg);
		if (ret) {
			pr_err("%s: msm8x16_wcd_hs_detect failed\n", __func__);
			kfree(mbhc_cfg.calibration);
			return ret;
		}
	}
	return msm8x16_wcd_hs_detect(codec, &mbhc_cfg);
#else
	return 0;
#endif /* CONFIG_SND_SOC_WCD_MBHC */
	}

static struct snd_soc_ops msm8952_quat_mi2s_be_ops = {
	.startup = msm_quat_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_quat_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8952_quin_mi2s_be_ops = {
	.startup = msm_quin_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_quin_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8952_sec_mi2s_be_ops = {
	.startup = msm_sec_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_sec_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8952_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_pri_auxpcm_be_ops = {
	.startup = msm_prim_auxpcm_startup,
	.shutdown = msm_prim_auxpcm_shutdown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8952_dai[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = "MSM8952 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{/* hw:x,1 */
		.name = "MSM8952 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{/* hw:x,2 */
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,3 */
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{/* hw:x,4 */
		.name = "MSM8X16 ULL",
		.stream_name = "ULL",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-dsp.2",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{/* hw:x,5 */
		.name = "Primary MI2S_RX Hostless",
		.stream_name = "Primary MI2S_RX Hostless",
		.cpu_dai_name = "PRI_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		/* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,6 */
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,7 */
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{/* hw:x,8 */
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{/* hw:x,9 */
		.name = "MSM8952 Compress1",
		.stream_name = "Compress1",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{/* hw:x,10 */
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,11 */
		.name = "Tertiary MI2S_TX Hostless",
		.stream_name = "Tertiary MI2S_TX Hostless",
		.cpu_dai_name = "TERT_MI2S_TX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,12 */
		.name = "MSM8x16 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	{/* hw:x,13 */
		.name = "Voice2",
		.stream_name = "Voice2",
		.cpu_dai_name   = "Voice2",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICE2,
	},
	{/* hw:x,14 */
		.name = "MSM8x16 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name   = "MultiMedia9",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* This dailink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
	{ /* hw:x,15 */
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOLTE,
	},
	{ /* hw:x,16 */
		.name = "VoWLAN",
		.stream_name = "VoWLAN",
		.cpu_dai_name   = "VoWLAN",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOWLAN,
	},
	{/* hw:x,17 */
		.name = "INT_HFP_BT Hostless",
		.stream_name = "INT_HFP_BT Hostless",
		.cpu_dai_name = "INT_HFP_BT_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,18 */
		.name = "MSM8916 HFP",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	/* LSM FE */
	{/* hw:x,19 */
		.name = "Listen 1 Audio Service",
		.stream_name = "Listen 1 Audio Service",
		.cpu_dai_name = "LSM1",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM1,
	},
	{/* hw:x,20 */
		.name = "Listen 2 Audio Service",
		.stream_name = "Listen 2 Audio Service",
		.cpu_dai_name = "LSM2",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM2,
	},
	{/* hw:x,21 */
		.name = "Listen 3 Audio Service",
		.stream_name = "Listen 3 Audio Service",
		.cpu_dai_name = "LSM3",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM3,
	},
	{/* hw:x,22 */
		.name = "Listen 4 Audio Service",
		.stream_name = "Listen 4 Audio Service",
		.cpu_dai_name = "LSM4",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM4,
	},
	{/* hw:x,23 */
		.name = "Listen 5 Audio Service",
		.stream_name = "Listen 5 Audio Service",
		.cpu_dai_name = "LSM5",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM5,
	},
	{ /* hw:x,24 */
		.name = "MSM8X16 Compress2",
		.stream_name = "Compress2",
		.cpu_dai_name   = "MultiMedia7",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{ /* hw:x,25 */
		.name = "QUAT_MI2S Hostless",
		.stream_name = "QUAT_MI2S Hostless",
		.cpu_dai_name = "QUAT_MI2S_RX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,26 */
		.name = LPASS_BE_SENARY_MI2S_TX,
		.stream_name = "Senary_mi2s Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.6",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.be_id = MSM_BACKEND_DAI_SENARY_MI2S_TX,
		.be_hw_params_fixup = msm_senary_tx_be_hw_params_fixup,
		.ops = &msm8952_mi2s_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.ignore_pmdown_time = 1,
	},
	{/* hw:x,27 */
		.name = "MSM8X16 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name	= "MultiMedia10",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{/* hw:x,28 */
		.name = "MSM8X16 Compress4",
		.stream_name = "Compress4",
		.cpu_dai_name	= "MultiMedia11",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{/* hw:x,29 */
		.name = "MSM8X16 Compress5",
		.stream_name = "Compress5",
		.cpu_dai_name	= "MultiMedia12",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{/* hw:x,30 */
		.name = "MSM8X16 Compress6",
		.stream_name = "Compress6",
		.cpu_dai_name	= "MultiMedia13",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{/* hw:x,31 */
		.name = "MSM8X16 Compress7",
		.stream_name = "Compress7",
		.cpu_dai_name	= "MultiMedia14",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{/* hw:x,32 */
		.name = "MSM8X16 Compress8",
		.stream_name = "Compress8",
		.cpu_dai_name	= "MultiMedia15",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{/* hw:x,33 */
		.name = "MSM8X16 Compress9",
		.stream_name = "Compress9",
		.cpu_dai_name	= "MultiMedia16",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	{/* hw:x,34 */
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name   = "VoiceMMode1",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{/* hw:x,35 */
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name   = "VoiceMMode2",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	{/* hw:x,36 */
		.name = "MSM8916 HFP Loopback2",
		.stream_name = "MultiMedia8",
		.cpu_dai_name = "MultiMedia8",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	{ /* hw:x,37 */
		.name = "QUIN_MI2S Hostless",
		.stream_name = "QUIN_MI2S Hostless",
		.cpu_dai_name = "QUIN_MI2S_RX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
#if defined(CONFIG_SND_SOC_JACK_AUDIO)
	{
		.name = "MSM8952 JACK LowLatency",
		.stream_name = "MultiMedia17",
		.cpu_dai_name	= "MultiMedia17",
		.platform_name	= "msm-pcm-dsp.1",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA17,
	},
#endif
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_pri_rx_be_hw_params_fixup,
		.ops = &msm8952_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_MI2S_RX,
		.stream_name = "Secondary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_sec_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.be_hw_params_fixup = msm_tx_be_hw_params_fixup,
		.ops = &msm8952_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_quat_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_quat_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	/* Primary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_FM_TX,
		.stream_name = "Internal FM Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12293",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm_proxy_rx_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6-dev.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_proxy_tx_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music 2 BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE2_PLAYBACK_TX,
		.stream_name = "Voice2 Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32770",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},

	{
		.name = LPASS_BE_QUIN_MI2S_TX,
		.stream_name = "Quinary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.5",
		.platform_name = "msm-pcm-routing",
#if defined(CONFIG_SND_SOC_CS47L15)
		.codec_name = "cs47l15-codec",
		.codec_dai_name = "cs47l15-aif1",
		.dai_fmt = SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF,
		.init = &msm_madera_init,
#else		
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
#endif
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_quin_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};
static struct snd_soc_dai_link msm8952_hdmi_dba_dai_link[] = {
	{
		.name = LPASS_BE_QUIN_MI2S_RX,
		.stream_name = "Quinary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.5",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "msm_hdmi_dba_codec_rx_dai",
		.codec_name = "msm-hdmi-dba-codec-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_quin_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8952_quin_dai_link[] = {
	{
		.name = LPASS_BE_QUIN_MI2S_RX,
		.stream_name = "Quinary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.5",
		.platform_name = "msm-pcm-routing",
	#if defined(CONFIG_SND_SOC_CS47L15)
		.codec_name = "cs47l15-codec",
		.codec_dai_name = "cs47l15-aif1",
		.dai_fmt = SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF,
		.init = &msm_madera_init,
        #else
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	#endif
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_quin_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8952_split_a2dp_dai_link[] = {
	{
		.name = LPASS_BE_INT_BT_A2DP_RX,
		.stream_name = "Internal BT-A2DP Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12290",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "msm-stub-rx",
		.codec_name = "msm-stub-codec.1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_A2DP_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8952_dai_links[
ARRAY_SIZE(msm8952_dai) +
ARRAY_SIZE(msm8952_hdmi_dba_dai_link) +
ARRAY_SIZE(msm8952_split_a2dp_dai_link)];

static int msm8952_wsa881x_init(struct snd_soc_component *component)
{
	return 0;
}

static struct snd_soc_aux_dev msm8952_aux_dev[] = {
	{
		.name = "wsa881x.0",
		.codec_name = NULL,
		.init = msm8952_wsa881x_init,
	},
	{
		.name = "wsa881x.0",
		.codec_name = NULL,
		.init = msm8952_wsa881x_init,
	},
};

static struct snd_soc_codec_conf msm8952_codec_conf[] = {
	{
		.dev_name = NULL,
		.name_prefix = NULL,
	},
	{
		.dev_name = NULL,
		.name_prefix = NULL,
	},
};

static struct snd_soc_card bear_card = {
	/* snd_soc_card_msm8952 */
	.name		= "msm8952-snd-cs47l15-card",
	.dai_link	= msm8952_dai,
	.num_links	= ARRAY_SIZE(msm8952_dai),
	#if defined(CONFIG_SND_SOC_CS47L15)
	.late_probe = msm8952_cs47l15_late_probe,	
	.dapm_widgets = cs47l15_widgets,	
	.num_dapm_widgets = ARRAY_SIZE(cs47l15_widgets),	
	.dapm_routes = msm8952_cs47l15_routes,	
	.num_dapm_routes = ARRAY_SIZE(msm8952_cs47l15_routes),	
	.set_bias_level = msm8952_cs47l15_set_bias_level,	
	.set_bias_level_post = msm8952_cs47l15_set_bias_level_post,
      #endif
};

void msm8952_disable_cs47l15_mclk(struct work_struct *work)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct delayed_work *dwork;
	int ret = 0;

	dwork = to_delayed_work(work);
	pdata = container_of(dwork, struct msm8916_asoc_mach_data,
			disable_mclk_work);
	mutex_lock(&pdata->cdc_mclk_mutex);
	pr_debug("%s: mclk_enabled %d mclk_rsc_ref %d\n", __func__,
			atomic_read(&pdata->mclk_enabled),
			atomic_read(&pdata->mclk_rsc_ref));

	if (atomic_read(&pdata->mclk_enabled) == true
			&& atomic_read(&pdata->mclk_rsc_ref) == 0) {
		pr_debug("Disable the mclk\n");
		if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
			pdata->digital_cdc_clk.clk_val = 0;
			ret = afe_set_digital_codec_core_clock(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
				&pdata->digital_cdc_clk);

		} else {
			pdata->digital_cdc_core_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
				&pdata->digital_cdc_core_clk);
		}
		if (ret < 0)
			pr_err("%s failed to disable the CCLK\n", __func__);
		atomic_set(&pdata->mclk_enabled, false);
	}
	mutex_unlock(&pdata->cdc_mclk_mutex);
}

#ifdef CONFIG_SND_SOC_WCD_MBHC
static bool msm8952_swap_gnd_mic(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int value, ret;

	pr_debug("%s: configure gpios for US_EU\n", __func__);

	if (!gpio_is_valid(pdata->us_euro_gpio)) {
		pr_err("%s: Invalid gpio: %d", __func__, pdata->us_euro_gpio);
		return false;
	}
	value = gpio_get_value_cansleep(pdata->us_euro_gpio);
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "us_eu_gpio");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be activated %sd",
				__func__, "us_eu_gpio");
		return false;
	}
	gpio_set_value_cansleep(pdata->us_euro_gpio, !value);
	pr_debug("%s: swap select switch %d to %d\n", __func__, value, !value);

	ret = msm_gpioset_suspend(CLIENT_WCD_INT, "us_eu_gpio");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be de-activated %sd",
				__func__, "us_eu_gpio");
		return false;
	}

	return true;
}
#endif /* CONFIG_SND_SOC_WCD_MBHC */

static void msm8952_dt_parse_cap_info(struct platform_device *pdev,
		struct msm8916_asoc_mach_data *pdata)
{
	const char *ext1_cap = "qcom,msm-micbias1-ext-cap";
	const char *ext2_cap = "qcom,msm-micbias2-ext-cap";

	pdata->micbias1_cap_mode =
		(of_property_read_bool(pdev->dev.of_node, ext1_cap) ?
		 MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);

	pdata->micbias2_cap_mode =
		(of_property_read_bool(pdev->dev.of_node, ext2_cap) ?
		 MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
}


static int msm8952_populate_dai_link_component_of_node(
		struct snd_soc_card *card)
{
	int i, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *phandle;

	if (!cdev) {
		pr_err("%s: Sound card device memory NULL\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < card->num_links; i++) {
		if (dai_link[i].platform_of_node && dai_link[i].cpu_of_node)
			continue;

		/* populate platform_of_node for snd card dai links */
		if (dai_link[i].platform_name &&
				!dai_link[i].platform_of_node) {
			index = of_property_match_string(cdev->of_node,
					"asoc-platform-names",
					dai_link[i].platform_name);
			if (index < 0) {
				pr_err("%s: No match found for platform name: %s\n",
					__func__, dai_link[i].platform_name);
				ret = index;
				goto cpu_dai;
			}
			phandle = of_parse_phandle(cdev->of_node,
					"asoc-platform",
					index);
			if (!phandle) {
				pr_err("%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platform_name,
						index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platform_of_node = phandle;
			dai_link[i].platform_name = NULL;
		}
cpu_dai:
		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpu_dai_name && !dai_link[i].cpu_of_node) {
			index = of_property_match_string(cdev->of_node,
					"asoc-cpu-names",
					dai_link[i].cpu_dai_name);
			if (index < 0)
				goto codec_dai;
			phandle = of_parse_phandle(cdev->of_node, "asoc-cpu",
					index);
			if (!phandle) {
				pr_err("%s: retrieving phandle for cpu dai %s failed\n",
					__func__, dai_link[i].cpu_dai_name);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].cpu_of_node = phandle;
			dai_link[i].cpu_dai_name = NULL;
		}
codec_dai:
		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].codec_name && !dai_link[i].codec_of_node) {
			index = of_property_match_string(cdev->of_node,
					"asoc-codec-names",
					dai_link[i].codec_name);
			if (index < 0)
				continue;
			phandle = of_parse_phandle(cdev->of_node, "asoc-codec",
					index);
			if (!phandle) {
				pr_err("%s: retrieving phandle for codec dai %s failed\n",
					__func__, dai_link[i].codec_name);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].codec_of_node = phandle;
			dai_link[i].codec_name = NULL;
		}
	}
err:
	return ret;
}

#ifdef CONFIG_SAMSUNG_JACK
void msm8952_set_micbias(bool state)
{
	struct snd_soc_jack *jack = &hs_jack;
	struct snd_soc_codec *codec;
	struct snd_soc_dapm_context *dapm;
	char *str = "MIC BIAS Power External2";
	int ret = 0;

	if (jack->codec == NULL) {
		pr_err("%s codec is NULL\n", __func__);
		return;
	}
	pr_info("%s : %s, state=%d\n", __func__, str, state);

	mutex_lock(&jack_mutex);
	codec = jack->codec;
	dapm = &codec->dapm;

	if (state == true)
		ret = snd_soc_dapm_force_enable_pin(dapm, str);
	else
		ret = snd_soc_dapm_disable_pin(dapm, str);

	if(ret < 0) {
		pr_err("%s is failed(%d)\n",
			__func__, ret);
	}

	snd_soc_dapm_sync(dapm);
	mutex_unlock(&jack_mutex);
}

static int msm8952_get_adc(void)
{
	struct snd_soc_jack *jack = &hs_jack;
	struct snd_soc_codec *codec;
	struct msm8916_asoc_mach_data *pdata;
	struct qpnp_vadc_result result;
	struct qpnp_vadc_chip *earjack_vadc;
	uint32_t mpp_ch;
	int adc;

	if (jack->codec == NULL) {
		pr_err("%s codec==NULL\n", __func__);
		return -1;
	}
	codec = jack->codec;
	pdata = snd_soc_card_get_drvdata(codec->component.card);

	mpp_ch = pdata->mpp_ch_scale[0] + P_MUX1_1_3 - 1;

	if (pdata->mpp_ch_scale[2] == 1)
		mpp_ch = pdata->mpp_ch_scale[0] + P_MUX1_1_1 - 1;
	else if (pdata->mpp_ch_scale[2] == 3)
		mpp_ch = pdata->mpp_ch_scale[0] + P_MUX1_1_3 - 1;
	else
		pr_err("%s - invalid channel scale=%d\n",
			__func__, pdata->mpp_ch_scale[2]);

#ifdef CONFIG_SEC_MPP_SHARE
	if (!pdata->mpp_standalone_mode)
		sec_mpp_mux_control(EAR_ADC_MUX_SEL_NUM,
			SEC_MUX_SEL_EAR_ADC, 1);
#endif /* CONFIG_SEC_MPP_SHARE */
	earjack_vadc = qpnp_get_vadc(codec->component.card->dev,
		"earjack-read");
	qpnp_vadc_read(earjack_vadc,  mpp_ch, &result);
#ifdef CONFIG_SEC_MPP_SHARE
	if (!pdata->mpp_standalone_mode)
		sec_mpp_mux_control(EAR_ADC_MUX_SEL_NUM,
			SEC_MUX_SEL_EAR_ADC, 0);
#endif /* CONFIG_SEC_MPP_SHARE */

	/* Get voltage in microvolts */
	adc = ((int)result.physical)/1000;

	return adc;
}
#endif /* CONFIG_SAMSUNG_JACK */

int msm8952_init_wsa_switch_supply(struct platform_device *pdev,
		struct msm8916_asoc_mach_data *pdata)
{
	const char *switch_supply_str = "msm-vdd-wsa-switch";
	char prop_name[MSM_DT_MAX_PROP_SIZE];
	struct device_node *regnode = NULL;
	struct device *dev = &pdev->dev;
	u32 prop_val;
	int ret = 0;

	snprintf(prop_name, MSM_DT_MAX_PROP_SIZE, "%s-supply",
		switch_supply_str);
	regnode = of_parse_phandle(dev->of_node, prop_name, 0);
	if (!regnode) {
		dev_err(dev, "Looking up %s property in node %s failed\n",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}

	pdata->wsa_switch_supply.supply = devm_regulator_get(dev,
			switch_supply_str);
	if (IS_ERR(pdata->wsa_switch_supply.supply)) {
		ret = PTR_ERR(pdata->wsa_switch_supply.supply);
		dev_err(dev, "Failed to get wsa switch supply: err = %d\n",
					ret);
		return ret;
	}

	snprintf(prop_name, MSM_DT_MAX_PROP_SIZE,
		"qcom,%s-voltage", switch_supply_str);
	ret = of_property_read_u32(dev->of_node, prop_name,
			&prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -EFAULT;
	}
	ret = regulator_set_voltage(pdata->wsa_switch_supply.supply,
		prop_val, prop_val);
	if (ret) {
		dev_err(dev, "Setting voltage failed for regulator %s err = %d\n",
			switch_supply_str, ret);
		pdata->wsa_switch_supply.supply = NULL;
		return ret;
	}

	snprintf(prop_name, MSM_DT_MAX_PROP_SIZE,
		"qcom,%s-current", switch_supply_str);
	ret = of_property_read_u32(dev->of_node, prop_name,
			&prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -EFAULT;
	}
	ret = regulator_set_optimum_mode(pdata->wsa_switch_supply.supply,
		prop_val);
	if (ret < 0) {
		dev_err(dev, "Setting current failed for regulator %s err = %d\n",
			switch_supply_str, ret);
		pdata->wsa_switch_supply.supply = NULL;
		return ret;
	}

	atomic_set(&pdata->wsa_switch_supply.ref, 0);
	return ret;
}

static struct snd_soc_card *msm8952_populate_sndcard_dailinks(
						struct device *dev)
{
	struct snd_soc_card *card = &bear_card;
	struct snd_soc_dai_link *dailink;
	int len1;

	card->name = dev_name(dev);
	len1 = ARRAY_SIZE(msm8952_dai);
	memcpy(msm8952_dai_links, msm8952_dai, sizeof(msm8952_dai));
	dailink = msm8952_dai_links;
	if (of_property_read_bool(dev->of_node,
				"qcom,hdmi-dba-codec-rx")) {
		dev_dbg(dev, "%s(): hdmi audio support present\n",
				__func__);
		memcpy(dailink + len1, msm8952_hdmi_dba_dai_link,
				sizeof(msm8952_hdmi_dba_dai_link));
		len1 += ARRAY_SIZE(msm8952_hdmi_dba_dai_link);
	} else {
		dev_dbg(dev, "%s(): No hdmi dba present, add quin dai\n",
				__func__);
		memcpy(dailink + len1, msm8952_quin_dai_link,
				sizeof(msm8952_quin_dai_link));
		len1 += ARRAY_SIZE(msm8952_quin_dai_link);
	}
	if (of_property_read_bool(dev->of_node,
				"qcom,split-a2dp")) {
		dev_dbg(dev, "%s(): split a2dp support present\n",
				__func__);
		memcpy(dailink + len1, msm8952_split_a2dp_dai_link,
				sizeof(msm8952_split_a2dp_dai_link));
		len1 += ARRAY_SIZE(msm8952_split_a2dp_dai_link);
	}
	card->dai_link = dailink;
	card->num_links = len1;
	return card;
}

static int msm8952_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm8916_asoc_mach_data *pdata = NULL;
	const char *hs_micbias_type = "qcom,msm-hs-micbias-type";
	const char *ext_pa = "qcom,msm-ext-pa";
	const char *mclk = "qcom,msm-mclk-freq";
#ifdef CONFIG_SND_SOC_WSA881X_ANALOG
	const char *wsa = "asoc-wsa-codec-names";
	const char *wsa_prefix = "asoc-wsa-codec-prefixes";
	const char *wsa_str = NULL;
	const char *wsa_prefix_str = NULL;
	char *temp_str = NULL;
#endif /* CONFIG_SND_SOC_WSA881X_ANALOG */
	const char *type = NULL;
	const char *ext_pa_str = NULL;
	int num_strings;
	int ret, id, i, val;
	struct resource	*muxsel;

      lochnagar_notifier.dev = &pdev->dev;
	 
	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm8916_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_mic_ctl");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err1;
	}
	pdata->vaddr_gpio_mux_mic_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_mic_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err1;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_spkr_ctl");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_spkr_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_spkr_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_lpaif_pri_pcm_pri_mode_muxsel");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_pcm_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_pcm_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_quin_ctl");
	if (!muxsel) {
		dev_dbg(&pdev->dev, "MUX addr invalid for MI2S\n");
		goto parse_mclk_freq;
	}
	pdata->vaddr_gpio_mux_quin_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_quin_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err;
	}
parse_mclk_freq:
	ret = of_property_read_u32(pdev->dev.of_node, mclk, &id);
	if (ret) {
		dev_err(&pdev->dev,
				"%s: missing %s in dt node\n", __func__, mclk);
		id = DEFAULT_MCLK_RATE;
	}
	pdata->mclk_freq = id;

	/*reading the gpio configurations from dtsi file*/
	ret = msm_gpioset_initialize(CLIENT_WCD_INT, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: error reading dtsi files%d\n", __func__, ret);
		goto err;
	}
#ifdef CONFIG_SND_SOC_WSA881X_ANALOG
	num_strings = of_property_count_strings(pdev->dev.of_node,
			wsa);
	if (num_strings > 0) {
		if (wsa881x_get_probing_count() < 2) {
			ret = -EPROBE_DEFER;
			goto err;
		} else if (wsa881x_get_presence_count() == num_strings) {
			bear_card.aux_dev = msm8952_aux_dev;
			bear_card.num_aux_devs = num_strings;
			bear_card.codec_conf = msm8952_codec_conf;
			bear_card.num_configs = num_strings;

			for (i = 0; i < num_strings; i++) {
				ret = of_property_read_string_index(
						pdev->dev.of_node, wsa,
						i, &wsa_str);
				if (ret) {
					dev_err(&pdev->dev,
						"%s:of read string %s i %d error %d\n",
						__func__, wsa, i, ret);
					goto err;
				}
				temp_str = kstrdup(wsa_str, GFP_KERNEL);
				if (!temp_str) {
					ret = -ENOMEM;
					goto err;
				}
				msm8952_aux_dev[i].codec_name = temp_str;
				temp_str = NULL;

				temp_str = kstrdup(wsa_str, GFP_KERNEL);
				if (!temp_str) {
					ret = -ENOMEM;
					goto err;
				}
				msm8952_codec_conf[i].dev_name = temp_str;
				temp_str = NULL;

				ret = of_property_read_string_index(
						pdev->dev.of_node, wsa_prefix,
						i, &wsa_prefix_str);
				if (ret) {
					dev_err(&pdev->dev,
						"%s:of read string %s i %d error %d\n",
						__func__, wsa_prefix, i, ret);
					goto err;
				}
				temp_str = kstrdup(wsa_prefix_str, GFP_KERNEL);
				if (!temp_str) {
					ret = -ENOMEM;
					goto err;
				}
				msm8952_codec_conf[i].name_prefix = temp_str;
				temp_str = NULL;
			}

			ret = msm8952_init_wsa_switch_supply(pdev, pdata);
			if (ret < 0) {
				pr_err("%s: failed to init wsa_switch vdd supply %d\n",
						__func__, ret);
				goto err;
			}
			wsa881x_set_mclk_callback(msm8952_enable_wsa_mclk);
			/* update the internal speaker boost usage */
			msm8x16_update_int_spk_boost(false);
		}
	}
#endif /* CONFIG_SND_SOC_WSA881X_ANALOG */
	card = msm8952_populate_sndcard_dailinks(&pdev->dev);
	dev_info(&pdev->dev, "default codec CS47L15 configured\n");
	num_strings = of_property_count_strings(pdev->dev.of_node,
			ext_pa);
	if (num_strings < 0) {
		dev_err(&pdev->dev,
				"%s: missing %s in dt node or length is incorrect\n",
				__func__, ext_pa);
		goto err;
	}
	for (i = 0; i < num_strings; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
				ext_pa, i, &ext_pa_str);
		if (ret) {
			dev_err(&pdev->dev, "%s:of read string %s i %d error %d\n",
					__func__, ext_pa, i, ret);
			goto err;
		}
		if (!strcmp(ext_pa_str, "primary"))
			pdata->ext_pa = (pdata->ext_pa | PRI_MI2S_ID);
		else if (!strcmp(ext_pa_str, "secondary"))
			pdata->ext_pa = (pdata->ext_pa | SEC_MI2S_ID);
		else if (!strcmp(ext_pa_str, "tertiary"))
			pdata->ext_pa = (pdata->ext_pa | TER_MI2S_ID);
		else if (!strcmp(ext_pa_str, "quaternary"))
			pdata->ext_pa = (pdata->ext_pa | QUAT_MI2S_ID);
		else if (!strcmp(ext_pa_str, "quinary"))
			pdata->ext_pa = (pdata->ext_pa | QUIN_MI2S_ID);
	}
	pr_debug("%s: ext_pa = %d\n", __func__, pdata->ext_pa);
#ifdef CONFIG_SND_SOC_WCD_MBHC
	ret = is_us_eu_switch_gpio_support(pdev, pdata);
	if (ret < 0) {
		pr_err("%s: failed to is_us_eu_switch_gpio_support %d\n",
				__func__, ret);
		goto err;
	}
#endif /* CONFIG_SND_SOC_WCD_MBHC */
	ret = is_ext_spk_gpio_support(pdev, pdata);
	if (ret < 0)
		pr_err("%s:  doesn't support external speaker pa\n",
				__func__);

	ret = of_property_read_string(pdev->dev.of_node,
		hs_micbias_type, &type);
	if (ret) {
		dev_err(&pdev->dev, "%s: missing %s in dt node\n",
			__func__, hs_micbias_type);
		goto err;
	}
#ifdef CONFIG_SND_SOC_WCD_MBHC
	if (!strcmp(type, "external")) {
		dev_dbg(&pdev->dev, "Headset is using external micbias\n");
		mbhc_cfg.hs_ext_micbias = true;
	} else {
		dev_dbg(&pdev->dev, "Headset is using internal micbias\n");
		mbhc_cfg.hs_ext_micbias = false;
	}
#endif  /* CONFIG_SND_SOC_WCD_MBHC */

	ret = of_property_read_u32(pdev->dev.of_node,
				  "qcom,msm-afe-clk-ver", &val);
	if (ret)
		pdata->afe_clk_ver = AFE_CLK_VERSION_V2;
	else
		pdata->afe_clk_ver = val;
	/* initialize the mclk */
	pdata->digital_cdc_clk.i2s_cfg_minor_version =
					AFE_API_VERSION_I2S_CONFIG;
	pdata->digital_cdc_clk.clk_val = pdata->mclk_freq;
	pdata->digital_cdc_clk.clk_root = 5;
	pdata->digital_cdc_clk.reserved = 0;
	/* initialize the digital codec core clk */
	pdata->digital_cdc_core_clk.clk_set_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
	pdata->digital_cdc_core_clk.clk_id =
			Q6AFE_LPASS_CLK_ID_INTERNAL_DIGITAL_CODEC_CORE;
	pdata->digital_cdc_core_clk.clk_freq_in_hz =
			pdata->mclk_freq;
	pdata->digital_cdc_core_clk.clk_attri =
			Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO;
	pdata->digital_cdc_core_clk.clk_root =
			Q6AFE_LPASS_CLK_ROOT_DEFAULT;
	pdata->digital_cdc_core_clk.enable = 1;

	/* Initialize loopback mode to false */
	pdata->lb_mode = false;

	msm8952_dt_parse_cap_info(pdev, pdata);

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;
	/* initialize timer */
	INIT_DELAYED_WORK(&pdata->disable_mclk_work, msm8952_disable_cs47l15_mclk);
	mutex_init(&pdata->cdc_mclk_mutex);
	atomic_set(&pdata->mclk_rsc_ref, 0);
	if (card->aux_dev) {
		mutex_init(&pdata->wsa_mclk_mutex);
		atomic_set(&pdata->wsa_mclk_rsc_ref, 0);
	}
	atomic_set(&pdata->mclk_enabled, false);
	atomic_set(&quat_mi2s_clk_ref, 0);
	atomic_set(&quin_mi2s_clk_ref, 0);
	atomic_set(&auxpcm_mi2s_clk_ref, 0);

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	ret = msm8952_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}
	
#if defined(CONFIG_SND_SOC_CS47L15)
	msm8952_update_impedance_table(pdev->dev.of_node);
	dev_info(&pdev->dev, "msm8952_update_impedance_table \n");
#endif

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);

#ifdef CONFIG_SAMSUNG_JACK
#ifdef CONFIG_SEC_MPP_SHARE
	pdata->mpp_standalone_mode = of_property_read_bool(pdev->dev.of_node,
		"qcom,mpp-standalone-mode");
#endif /* CONFIG_SEC_MPP_SHARE */

	ret = of_property_read_u32_array(pdev->dev.of_node,
		"qcom,mpp-channel-scaling", pdata->mpp_ch_scale, 3);
	if (ret < 0) {
		dev_info(&pdev->dev, "can`t find mpp-ch from the dt\n");
		pdata->mpp_ch_scale[0] = 2;
		pdata->mpp_ch_scale[1] = 1;
		pdata->mpp_ch_scale[2] = 1;
	}
	dev_dbg(&pdev->dev, "mpp-channel-scaling - %d %d %d\n",
		pdata->mpp_ch_scale[0],
		pdata->mpp_ch_scale[1],
		pdata->mpp_ch_scale[2]);

	jack_controls.set_micbias = msm8952_set_micbias;
	jack_controls.get_adc = msm8952_get_adc;
	jack_controls.snd_card_registered = 1;

	mutex_init(&jack_mutex);
#endif /* CONFIG_SAMSUNG_JACK */

if (!of_find_property(pdev->dev.of_node, "clock-names", NULL)) {
		printk("%s: codec not using audio-ext-clk driver\n",
			__func__);
	} else {
		printk( "%s: codec using audio-ext-clk driver\n",
			__func__);
		cs47l15_ext_clk = clk_get(&pdev->dev, "ext-mclk");
		if (IS_ERR(cs47l15_ext_clk)) {
			printk( "%s: clk get %s failed\n",
			__func__, "cs47l15_ext_clk");
		}
	}

	return 0;
err:
	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_quin_ctl)
		iounmap(pdata->vaddr_gpio_mux_quin_ctl);
	if (bear_card.num_aux_devs > 0) {
		for (i = 0; i < bear_card.num_aux_devs; i++) {
			kfree(msm8952_aux_dev[i].codec_name);
			kfree(msm8952_codec_conf[i].dev_name);
			kfree(msm8952_codec_conf[i].name_prefix);
		}
	}
err1:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm8952_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int i;

	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_quin_ctl)
		iounmap(pdata->vaddr_gpio_mux_quin_ctl);
	if (bear_card.num_aux_devs > 0) {
		for (i = 0; i < bear_card.num_aux_devs; i++) {
			kfree(msm8952_aux_dev[i].codec_name);
			kfree(msm8952_codec_conf[i].dev_name);
			kfree(msm8952_codec_conf[i].name_prefix);
		}
		mutex_destroy(&pdata->wsa_mclk_mutex);
	}
	snd_soc_unregister_card(card);
	mutex_destroy(&pdata->cdc_mclk_mutex);
	return 0;
}

static const struct of_device_id msm8952_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8952-cs47l15-audio-codec", },
	{},
};

static struct platform_driver msm8952_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8952_asoc_machine_of_match,
	},
	.probe = msm8952_asoc_machine_probe,
	.remove = msm8952_asoc_machine_remove,
};

static int __init msm8952_machine_init(void)
{
	return platform_driver_register(&msm8952_asoc_machine_driver);
}
late_initcall(msm8952_machine_init);

static void __exit msm8952_machine_exit(void)
{
	return platform_driver_unregister(&msm8952_asoc_machine_driver);
}
module_exit(msm8952_machine_exit);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8952_asoc_machine_of_match);
