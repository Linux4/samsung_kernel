/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/qpnp/clkdiv.h>
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
#include <sound/q6core.h>
#include <soc/qcom/socinfo.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "msm-audio-pinctrl.h"
#include "../codecs/msm8x16-wcd.h"
#include "../codecs/wsa881x-analog.h"
#include <linux/regulator/consumer.h>
#ifdef CONFIG_SAMSUNG_JACK
#include <linux/sec_jack.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/qpnp/pin.h>
#endif /* CONFIG_SAMSUNG_JACK */
#if defined(CONFIG_SEC_MPP_SHARE)
#include <linux/sec_mux_sel.h>
#endif
#define DRV_NAME "msm8952-asoc-cs47l15"
#if defined(CONFIG_SND_SOC_CS47L15)
#include "../codecs/arizona.h"
#include "../codecs/cs47l15.h"
#include <linux/mfd/arizona/pdata.h>
#include <linux/mfd/arizona/registers.h>
#include <sound/tlv.h>
#endif
#include <sound/pcm_params.h>

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

#define PRI_MI2S_ID	(1 << 0)
#define SEC_MI2S_ID	(1 << 1)
#define TER_MI2S_ID	(1 << 2)
#define QUAT_MI2S_ID	(1 << 3)
#define QUIN_MI2S_ID	(1 << 4)

#define DEFAULT_MCLK_RATE 9600000

#define WCD_MBHC_DEF_RLOADS 5
#define MAX_WSA_CODEC_NAME_LENGTH 80
#define MSM_DT_MAX_PROP_SIZE 80


enum btsco_rates {
	RATE_8KHZ_ID,
	RATE_16KHZ_ID,
};

static bool ext_codec;

static int msm8952_auxpcm_rate = 8000;
static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_btsco_ch = 1;
static int msm_ter_mi2s_tx_ch = 1;
static int msm_pri_mi2s_rx_ch = 1;
static int msm_proxy_rx_ch = 2;
static int msm_vi_feed_tx_ch = 2;
static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static atomic_t quat_mi2s_clk_ref;
static atomic_t quin_mi2s_clk_ref;
static atomic_t auxpcm_mi2s_clk_ref;

static int msm8952_enable_dig_cdc_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);

#if defined(CONFIG_SND_SOC_CS47L15)

int sample_rate_i2s_cs47l15 = 48000;
static struct snd_soc_card  *msm8952_cs47l15_card;
static DECLARE_TLV_DB_SCALE(digital_tlv,-6400,50,0);

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

static int msm8952_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);
static int msm8952_wsa_switch_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);
static int msm8952_ext_audio_switch_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);

#if defined(CONFIG_SAMSUNG_JACK)

//Enabling the MIC Bias Voltage of Earmic
static struct snd_soc_jack hs_jack;
static struct mutex jack_mutex;
#elif defined(CONFIG_SWITCH_ARIZONA)

#else
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

static struct afe_clk_cfg wsa_ana_clk_v1 = {
	AFE_API_VERSION_I2S_CONFIG,
	0,
	Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK2_VALID,
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
static const char *const ter_mi2s_tx_ch_text[] = {"One", "Two"};
static const char *const loopback_mclk_text[] = {"DISABLE", "ENABLE"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
struct clk *cs47l15_ext_clk;
unsigned long mclk_freq = 24000000;
bool mclk_enable = false;


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
	SND_SOC_DAPM_SUPPLY("VDD_EXT_AUDIO_SWITCH", SND_SOC_NOPM, 0, 0,
			    msm8952_ext_audio_switch_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
#if defined(CONFIG_SND_SOC_CS47L15)
	SND_SOC_DAPM_MIC("HeadsetMic", NULL),
	SND_SOC_DAPM_MIC("MainMic", NULL),
	SND_SOC_DAPM_MIC("SubMic", NULL),
#endif
};
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
int is_extspk_gpio_support(struct platform_device *pdev,
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

static int ext_audio_switch_support(struct platform_device *pdev,
			struct msm8916_asoc_mach_data *pdata)
{
	const char *ext_audio_switch = "qcom,msm-ext-audio-switch";
	const char *ext_switch_supply = "ext-switch-vdd";
	const char *ext_switch_voltage = "qcom,ext-switch-vdd-voltage";
	const char *ext_switch_op_cur = "qcom,ext-switch-vdd-op-mode";
	enum of_gpio_flags flags;
	int rc = -EINVAL;

	pdata->ext_audio_switch_gpio = of_get_named_gpio_flags
		(pdev->dev.of_node, ext_audio_switch, 0, &flags);

	if (pdata->ext_audio_switch_gpio < 0) {
		dev_dbg(&pdev->dev,
			"%s: missing %s in dt node\n",
			__func__, ext_audio_switch);
		goto err;
	} else {
		u32 minmax[2], op_mode;
		int len = 0;

		rc = gpio_is_valid(pdata->ext_audio_switch_gpio);
		if (!rc) {
			dev_err(&pdev->dev, "%s: Invalid audio switch gpio : %d",
				__func__, pdata->ext_audio_switch_gpio);
			goto err;
		}

		pdata->ext_audio_switch_active_high =
			!(flags & OF_GPIO_ACTIVE_LOW);
		pdata->ext_audio_switch_supply =
			devm_regulator_get(&pdev->dev, ext_switch_supply);

		if (IS_ERR(pdata->ext_audio_switch_supply)) {
			rc = PTR_ERR(pdata->ext_audio_switch_supply);
			dev_err(&pdev->dev, "%s: ext-switch-vdd-supply not defined: err:%d",
				__func__, rc);
			goto err;
		}

		if (of_get_property(pdev->dev.of_node,
				    ext_switch_voltage,
				    &len) && (len == sizeof(minmax))) {
			of_property_read_u32_array(pdev->dev.of_node,
						   ext_switch_voltage,
						   minmax, 2);
		} else {
			dev_err(&pdev->dev, "%s: voltage not properly defined",
				__func__);
			goto err;
		}

		rc = of_property_read_u32(pdev->dev.of_node,
					  ext_switch_op_cur, &op_mode);
		if (rc) {
			dev_err(&pdev->dev, "%s: op-mode current not defined",
				__func__);
			goto err;
		}
		rc = regulator_set_voltage(pdata->ext_audio_switch_supply,
					   minmax[0], minmax[1]);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Error setting ext_audio_switch_reg volt, err=%d\n",
				__func__, rc);
			goto  err;
		}

		rc = regulator_set_optimum_mode(pdata->ext_audio_switch_supply,
						op_mode);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"%s: Error setting optimum_mode, err=%d\n",
				__func__, rc);
			goto  err;
		}
		return 0;
	}
err:
	pdata->ext_audio_switch_supply = NULL;
	pdata->ext_audio_switch_gpio = -1;

	return -EINVAL;
}

static int enable_spk_ext_pa(struct snd_soc_codec *codec, int enable)
{
	struct snd_soc_card *card = codec->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;

	if (!gpio_is_valid(pdata->spk_ext_pa_gpio)) {
		pr_err("%s: Invalid gpio: %d\n", __func__,
			pdata->spk_ext_pa_gpio);
		return false;
	}

	pr_debug("%s: %s external speaker PA\n", __func__,
		enable ? "Enable" : "Disable");

	if (enable) {
		ret = msm_gpioset_activate(CLIENT_WCD_INT, "ext_spk_gpio");
		if (ret) {
			pr_err("%s: gpio set cannot be de-activated %s\n",
					__func__, "ext_spk_gpio");
			return ret;
		}
		gpio_set_value_cansleep(pdata->spk_ext_pa_gpio, enable);

		/* Some devices have secondary GPIO that needs to set */
		if (pdata->ext_audio_switch_gpio > 0) {
			gpio_set_value_cansleep(pdata->ext_audio_switch_gpio,
				pdata->ext_audio_switch_active_high);
		}
	} else {
		gpio_set_value_cansleep(pdata->spk_ext_pa_gpio, enable);
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "ext_spk_gpio");
		if (ret) {
			pr_err("%s: gpio set cannot be de-activated %s\n",
					__func__, "ext_spk_gpio");
			return ret;
		}
	}
	return 0;
}
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

	pr_debug("%s: Number of channels = %d\n", __func__,
			msm_pri_mi2s_rx_ch);
	rate->min = rate->max = 48000;
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


	if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
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
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       mi2s_rx_bit_format);
#if defined(CONFIG_SND_SOC_CS47L15)
	sample_rate_i2s_cs47l15 = params_rate(params);
#endif
	pr_debug("%s(): rate = %d, format = %d  params_format(params) = %d \n", __func__,sample_rate_i2s_cs47l15,mi2s_rx_bit_format, params_format(params));
	return 0;
}

#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
static int msm_q6_enable_mi2s_clocks(bool enable, int port_id)
{
	union afe_port_config port_config;
	int rc = 0;


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

#ifdef CONFIG_AUDIO_MSM8976LA10_COMPATIBLE
static int quat_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
	int ret = 0;

	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
				mi2s_rx_clk_v1.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
			else
				mi2s_rx_clk_v1.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk_v1);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk_v1.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_TX,
					&mi2s_tx_clk_v1);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

		
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
		if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			&& (pdata->ext_pa & QUAT_MI2S_ID))
			msm_q6_enable_mi2s_clocks(true, AFE_PORT_ID_QUATERNARY_MI2S_RX);
		
		if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			&& (pdata->ext_pa & QUAT_MI2S_ID))
			msm_q6_enable_mi2s_clocks(true, AFE_PORT_ID_QUATERNARY_MI2S_TX);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */

	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (pdata->ext_pa & QUAT_MI2S_ID)
				msm_q6_enable_mi2s_clocks(false, AFE_PORT_ID_QUATERNARY_MI2S_RX);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
			mi2s_rx_clk_v1.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk_v1);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (pdata->ext_pa & QUAT_MI2S_ID)
				msm_q6_enable_mi2s_clocks(false, AFE_PORT_ID_QUATERNARY_MI2S_TX);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
			mi2s_tx_clk_v1.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_TX,
					&mi2s_tx_clk_v1);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);
	}
	return ret;
}
static int quin_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
	int ret = 0;

	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
				mi2s_rx_clk_v1.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
			else
				mi2s_rx_clk_v1.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUINARY_MI2S_RX,
					&mi2s_rx_clk_v1);
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (pdata->ext_pa & QUIN_MI2S_ID)
				msm_q6_enable_mi2s_clocks(true, AFE_PORT_ID_QUINARY_MI2S_RX);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk_v1.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUINARY_MI2S_TX,
					&mi2s_tx_clk_v1);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (pdata->ext_pa & QUIN_MI2S_ID)
				msm_q6_enable_mi2s_clocks(false, AFE_PORT_ID_QUINARY_MI2S_RX);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
			mi2s_rx_clk_v1.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUINARY_MI2S_RX,
					&mi2s_rx_clk_v1);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk_v1.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUINARY_MI2S_TX,
					&mi2s_tx_clk_v1);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

	}
	return ret;
}
#endif
static uint32_t get_mi2s_rx_clk_val(int port_id)
{
	if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
		return Q6AFE_LPASS_IBIT_CLK_12_P288_MHZ;//Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
	else
		return Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
}

static int msm_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int port_id = 0;
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_card *card = rtd->card;
	//struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */

	port_id = msm8952_get_port_id(rtd->dai_link->be_id);
	pr_debug("msm_mi2s_sclk_ctl   port ID %x, enable :%d  %d\n", port_id, enable, (int) q6core_get_avs_version());

	if (port_id < 0) {
		pr_err("%s: Invalid port_id\n", __func__);
		return -EINVAL;
	}
	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
				mi2s_rx_clk_v1.clk_val1 =
						get_mi2s_rx_clk_val(port_id);
				ret = afe_set_lpass_clock(port_id,
							&mi2s_rx_clk_v1);
				break;
			case (Q6_SUBSYS_AVS2_7):
				mi2s_rx_clk.enable = enable;
				mi2s_rx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				mi2s_rx_clk.clk_freq_in_hz =
						get_mi2s_rx_clk_val(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_rx_clk);
				break;
			case (Q6_SUBSYS_INVALID):
			default:
				ret = -EINVAL;
				pr_err("%s: INVALID AVS IMAGE\n", __func__);
				break;
			}
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			 if (port_id == AFE_PORT_ID_QUATERNARY_MI2S_RX)
			    msm_q6_enable_mi2s_clocks(true, port_id);
#endif			
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
				mi2s_tx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_tx_clk_v1);
				break;
			case (Q6_SUBSYS_AVS2_7):
				mi2s_tx_clk.enable = enable;
				mi2s_tx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				mi2s_tx_clk.clk_freq_in_hz =
						get_mi2s_rx_clk_val(port_id);//Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_tx_clk);
				break;
			case (Q6_SUBSYS_INVALID):
			default:
				ret = -EINVAL;
				pr_err("%s: INVALID AVS IMAGE\n", __func__);
				break;
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

#if 0
		if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			&& (pdata->ext_pa & QUAT_MI2S_ID))
			msm_q6_enable_mi2s_clocks(true, AFE_PORT_ID_QUATERNARY_MI2S_RX);
		
		if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			&& (pdata->ext_pa & QUAT_MI2S_ID))
			msm_q6_enable_mi2s_clocks(true, AFE_PORT_ID_QUATERNARY_MI2S_TX);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */

	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (port_id == AFE_PORT_ID_QUATERNARY_MI2S_RX)
				msm_q6_enable_mi2s_clocks(false, port_id);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
				mi2s_rx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_rx_clk_v1);
				break;
			case (Q6_SUBSYS_AVS2_7):
				mi2s_rx_clk.enable = enable;
				mi2s_rx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_rx_clk);
				break;
			case (Q6_SUBSYS_INVALID):
			default:
				ret = -EINVAL;
				pr_err("%s: INVALID AVS IMAGE\n", __func__);
				break;
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
#ifdef CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE
			if (port_id == AFE_PORT_ID_QUATERNARY_MI2S_RX)
				msm_q6_enable_mi2s_clocks(false, port_id);
#endif /* CONFIG_AUDIO_SPEAKER_OUT_NXP_AMP_ENABLE */
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
				mi2s_tx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_tx_clk_v1);
				break;
			case (Q6_SUBSYS_AVS2_7):
				mi2s_tx_clk.enable = enable;
				mi2s_tx_clk.clk_id =
						msm8952_get_clk_id(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_tx_clk);
				break;
			case (Q6_SUBSYS_INVALID):
			default:
				ret = -EINVAL;
				pr_err("%s: INVALID AVS IMAGE\n", __func__);
				break;
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

	pdata = snd_soc_card_get_drvdata(codec->card);
	pr_debug("%s: codec name %s enable %d mclk ref counter %d\n",
		   __func__, codec->name, enable,
		   atomic_read(&pdata->mclk_rsc_ref));
	if (enable) {
		if (!atomic_read(&pdata->mclk_rsc_ref)) {
			cancel_delayed_work_sync(
					&pdata->disable_mclk_work);
			mutex_lock(&pdata->cdc_mclk_mutex);
			if (atomic_read(&pdata->mclk_enabled) == false) {
				switch (q6core_get_avs_version()) {
				case Q6_SUBSYS_AVS2_6:
					pdata->digital_cdc_clk.clk_val =
						pdata->mclk_freq;
					ret = afe_set_digital_codec_core_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_clk);
					break;
				case Q6_SUBSYS_AVS2_7:
					pdata->digital_cdc_core_clk.enable = 1;
					ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_core_clk);
					break;
				case Q6_SUBSYS_INVALID:
				default:
					ret = -EINVAL;
					pr_err("%s: enable clk failed\n",
								__func__);
					break;
				}
				if (ret < 0) {
					pr_err("%s: failed to enable MCLK\n",
							__func__);
					mutex_unlock(&pdata->cdc_mclk_mutex);
					return ret;
				}
				atomic_set(&pdata->mclk_enabled, true);
			}
			mutex_unlock(&pdata->cdc_mclk_mutex);
		}
		atomic_inc(&pdata->mclk_rsc_ref);
	} else {
		cancel_delayed_work_sync(&pdata->disable_mclk_work);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == true) {
			switch (q6core_get_avs_version()) {
			case Q6_SUBSYS_AVS2_6:
				pdata->digital_cdc_clk.clk_val = 0;
				ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
				break;
			case Q6_SUBSYS_AVS2_7:
				pdata->digital_cdc_core_clk.enable = 0;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_core_clk);
				break;
			case Q6_SUBSYS_INVALID:
			default:
			{
				ret = -EINVAL;
				pr_err("%s: disable clk failed\n",
							__func__);
				break;
			}
			}
			if (ret < 0)
				pr_err("%s: failed to disable MCLK\n",
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
		break;
	case 0:
	default:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
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
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pdata = snd_soc_card_get_drvdata(codec->card);
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
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
			{
				pdata->digital_cdc_clk.clk_val =
							pdata->mclk_freq;
				ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
					break;
			}
			case (Q6_SUBSYS_AVS2_7):
			{
				pdata->digital_cdc_core_clk.enable = 1;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_core_clk);
					break;
			}
			case (Q6_SUBSYS_INVALID):
			default:
			{
				ret = -EINVAL;
				pr_err("%s: unknown dsp image\n", __func__);
				break;
			}
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
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
			{
				pdata->digital_cdc_clk.clk_val = 0;
				ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
					break;
			}
			case (Q6_SUBSYS_AVS2_7):
			{
				pdata->digital_cdc_core_clk.enable = 0;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_core_clk);
					break;
			}
			case (Q6_SUBSYS_INVALID):
			default:
			{
				ret = -EINVAL;
				pr_err("%s: unknown dsp image\n", __func__);
				break;
			}
			}
			if (ret < 0) {
				pr_err("%s: failed to disable the MCLK: %d\n",
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
	SOC_ENUM_SINGLE_EXT(2, rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(2, ter_mi2s_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, loopback_mclk_text),
	SOC_ENUM_SINGLE_EXT(8, proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, vi_feed_ch_text),
};

static const char *const btsco_rate_text[] = {"BTSCO_RATE_8KHZ",
	"BTSCO_RATE_16KHZ"};
static const struct soc_enum msm_btsco_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
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
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_btsco_enum[0],
		     msm_btsco_rate_get, msm_btsco_rate_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[3],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", msm_snd_enum[4],
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),

};

static int msm8952_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	int ret = 0;

	pdata = snd_soc_card_get_drvdata(w->codec->card);
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

	pdata = snd_soc_card_get_drvdata(w->codec->card);
	supply = &pdata->wsa_switch_supply;
	if (!supply->supply) {
		dev_err(w->codec->card->dev, "%s: no wsa switch supply",
			__func__);
		return ret;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (atomic_inc_return(&supply->ref) == 1)
			ret = regulator_enable(supply->supply);
		if (ret)
			dev_err(w->codec->card->dev,
				"%s: Failed to enable wsa switch supply\n",
				__func__);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_read(&supply->ref) == 0) {
			dev_dbg(w->codec->card->dev,
				"%s: wsa switch supply has been disabled.\n",
				__func__);
			return ret;
		}
		if (atomic_dec_return(&supply->ref) == 0)
			ret = regulator_disable(supply->supply);
			if (ret)
				dev_err(w->codec->card->dev,
					"%s: Failed to disable wsa switch supply\n",
					__func__);
		break;
	default:
		break;
	}

	return ret;
}

static int msm8952_ext_audio_switch_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(w->codec->card);
	if (pdata->ext_audio_switch_supply == NULL)
		return ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = regulator_enable(pdata->ext_audio_switch_supply);
		if (ret)
			dev_err(w->codec->card->dev,
				"%s: Failed to enable ext audio switch supply\n",
				__func__);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = regulator_disable(pdata->ext_audio_switch_supply);
		if (ret)
			dev_err(w->codec->card->dev,
				"%s: Failed to disable ext audio switch supply\n",
				__func__);
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
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
			{
				wsa_ana_clk_v1.clk_val2 =
					Q6AFE_LPASS_OSR_CLK_9_P600_MHZ;
				ret = afe_set_lpass_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&wsa_ana_clk_v1);
					break;
			}
			case (Q6_SUBSYS_AVS2_7):
			{
				wsa_ana_clk.enable = enable;
				ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&wsa_ana_clk);
					break;
			}
			case (Q6_SUBSYS_INVALID):
			default:
			{
				ret = -EINVAL;
				pr_err("%s: unknown dsp image\n", __func__);
				break;
			}
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
			switch (q6core_get_avs_version()) {
			case (Q6_SUBSYS_AVS2_6):
			{
				wsa_ana_clk_v1.clk_val2 =
					Q6AFE_LPASS_OSR_CLK_DISABLE;
				ret = afe_set_lpass_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
						&wsa_ana_clk_v1);
					break;
			}
			case (Q6_SUBSYS_AVS2_7):
			{
				wsa_ana_clk.enable = enable;
				ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
					&wsa_ana_clk);
					break;
			}
			case (Q6_SUBSYS_INVALID):
			default:
			{
				ret = -EINVAL;
				pr_err("%s: unknown dsp image\n", __func__);
				break;
			}
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
		pr_debug("%s: set fmt cpu dai failed\n", __func__);
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
	if ((pdata->ext_pa & QUAT_MI2S_ID) == QUAT_MI2S_ID) {
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
	} else {
			pr_err("%s: error codec type\n", __func__);
	}
	if (atomic_inc_return(&quat_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_debug("%s: set fmt cpu dai failed\n", __func__);
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
	int ret =0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if ((pdata->ext_pa & QUAT_MI2S_ID) == QUAT_MI2S_ID) {
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

	//msm8952_cs47l15_set_mclk(1);
	
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
		if (ret < 0) {
			pr_debug("%s: set fmt cpu dai failed\n", __func__);
			goto err;
		}
	}
	return ret;
err:
	ret = msm_mi2s_sclk_ctl(substream, false);
	
	//msm8952_cs47l15_set_mclk(0);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_quin_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);

	//msm8952_cs47l15_set_mclk(0);
	
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

#if !defined(CONFIG_SAMSUNG_JACK)&&!defined(CONFIG_SWITCH_ARIZONA)
static void *def_msm8952_wcd_mbhc_cal(void)
{
	void *msm8952_wcd_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;

	msm8952_wcd_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!msm8952_wcd_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

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
#endif /* CONFIG_SAMSUNG_JACK */

#if defined(CONFIG_SND_SOC_CS47L15)
void msm8952_arizona_hpdet_cb(unsigned int meas)
{
	struct snd_soc_card *card = msm8952_cs47l15_card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);	
	int jack_det;
	int i, num_hp_gain_table;

	if (meas == ARIZONA_HP_Z_OPEN) {
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

void msm8952_arizona_micd_cb(bool mic)
{
	struct snd_soc_card *card = msm8952_cs47l15_card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pdata->ear_mic = mic;
	dev_info(card->dev, "%s: ear_mic = %d\n", __func__, pdata->ear_mic);
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

static int arizona_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_card *card = codec->card;
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


#if 0
static struct snd_soc_dapm_widget msm_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("HeadsetMic", NULL),
	SND_SOC_DAPM_MIC("MainMic", NULL),
	SND_SOC_DAPM_MIC("SubMic", NULL),
};


static struct snd_soc_dapm_route msm_cs47l15_routes[] = {
	{ "HeadsetMic", NULL, "MICBIAS1A" },
	{ "IN1BL", NULL, "HeadsetMic" },
	{ "MainMic", NULL, "MICBIAS1B" },
	{ "IN1AL", NULL, "MainMic" },
	{ "SubMic", NULL, "MICBIAS1C" },
	{ "IN1AR", NULL, "SubMic" },
};
#endif

static const struct snd_kcontrol_new msm_cs47l15_codec_controls[] = {
	SOC_SINGLE_EXT_TLV("HPOUT1L Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),

	SOC_SINGLE_EXT_TLV("HPOUT1R Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1R,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),
};

static int msm8952_cs47l15_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[54].codec;  //54 quin be dai
	int ret;

	msm8952_cs47l15_card = card;

	arizona_set_hpdet_cb(codec, msm8952_arizona_hpdet_cb);
	arizona_set_micd_cb(codec, msm8952_arizona_micd_cb);


	ret = snd_soc_add_codec_controls(codec, msm_cs47l15_codec_controls,
					ARRAY_SIZE(msm_cs47l15_codec_controls));

	dev_info(card->dev, "%s \n", __func__);

	return 0;
}

static int msm_set_bias_level(struct snd_soc_card *card,
				  struct snd_soc_dapm_context *dapm,
				  enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[54].codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	int ret;
	int sysclk, dspclk;
	unsigned int mclk, sample_rate, bits_per_sample;

	dev_dbg(card->dev, "%s: setup %s clocks ..LEVEL=%d\n", __func__, codec->name,level);
      
	if (dapm->dev != codec_dai->dev)
		return 0;

	sysclk = 98304000;
	dspclk = 147456000;
	mclk   = 24000000;
	sample_rate = sample_rate_i2s_cs47l15;


	if(192000 == sample_rate)
		bits_per_sample = 64;
	else
		bits_per_sample = 32;

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (dapm->bias_level != SND_SOC_BIAS_STANDBY)
			break;

		msm8952_cs47l15_set_mclk(1);

		dev_dbg(card->dev, "Setting SYSCLK to %u Hz\n", sysclk);

		ret  = snd_soc_codec_set_sysclk(codec,
						ARIZONA_CLK_SYSCLK,
						ARIZONA_CLK_SRC_FLL1,
						sysclk,
						SND_SOC_CLOCK_IN);
		if (ret != 0) {
			dev_err(card->dev, "Failed to set sysclk to %dHz: %d\n",sysclk, ret);
			return ret;
		}

		ret  = snd_soc_codec_set_sysclk(codec,
						ARIZONA_CLK_DSPCLK,
						ARIZONA_CLK_SRC_FLL1,
						dspclk,
						SND_SOC_CLOCK_IN);
		if (ret != 0) {
			dev_err(card->dev, "Failed to set dspclk to %dHz: %d\n",dspclk, ret);
			return ret;
		}

		/* Start FLL1 */
		ret = snd_soc_codec_set_pll(codec, CS47L15_FLL1_REFCLK,
						ARIZONA_FLL_SRC_MCLK1, mclk,
						sysclk);
		if (ret != 0) {
			dev_err(card->dev,"Failed to set FLL1 refclk to %dHz: %d\n", sysclk, ret);
			return ret;
		}

		ret = snd_soc_codec_set_pll(codec, CS47L15_FLL1,
						ARIZONA_FLL_SRC_AIF1BCLK, sample_rate * bits_per_sample,
						sysclk);
		if (ret != 0) {
			dev_err(card->dev,"Failed to set FLL1 to %dHz: %d\n", sysclk, ret);
			return ret;
		}

		break;
	default:
		break;
	}

	return 0;
}

static int msm_set_bias_level_post(struct snd_soc_card *card,
				       struct snd_soc_dapm_context *dapm,
				       enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[54].codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	int ret = 0;

	dev_dbg(card->dev, "%s: Reset %s clocks ..LEVEL=%d\n", __func__, codec->name,level);
	
	if (dapm->dev != codec_dai->dev)
		return ret;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		ret = snd_soc_codec_set_pll(codec, CS47L15_FLL1, 0, 0, 0);
		if (ret) {
			dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);
			return ret;
		}

		ret = snd_soc_codec_set_pll(codec, CS47L15_FLL1_REFCLK,
								ARIZONA_FLL_SRC_NONE, 0, 0);
		if (ret != 0) {
			dev_err(card->dev, "Failed to clear refclk: %d\n", ret);
			return ret;
		}

		msm8952_cs47l15_set_mclk(0);
		break;
	default:
		break;
	}

	dapm->bias_level = level;


	return 0;
}

static int msm_arizona_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *aif1_dai = rtd->codec_dai;
	struct snd_soc_dapm_context *codec_dapm = &codec->dapm; 
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dapm_context *dapm = &card->dapm;
	int ret;

	dev_info(rtd->card->dev, "%s: setup %s clocks\n", __func__, codec->name);

	ret = snd_soc_dai_set_sysclk(aif1_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret != 0) {
		dev_err(rtd->card->dev, "Failed to set AIF1 clock: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, CS47L15_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(rtd->card->dev, "Failed to clear refclk: %d\n", ret);
		return ret;
	}

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
	snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUT1L");
	snd_soc_dapm_ignore_suspend(codec_dapm,"HPOUT1R");
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1BL");
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1AL");
	snd_soc_dapm_ignore_suspend(codec_dapm,"IN1AR");
	
	snd_soc_dapm_sync(codec_dapm);

	return 0;
}
#endif

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = -ENOMEM;

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

	msm8x16_wcd_spk_ext_pa_cb(enable_spk_ext_pa, codec);
#if defined(CONFIG_SAMSUNG_JACK)
	hs_jack.codec = codec;
	ret = 0;
	return ret;
#elif defined(CONFIG_SWITCH_ARIZONA)	
	ret = 0;
	return ret;
#else
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
#endif /* CONFIG_SAMSUNG_JACK */
}

#ifdef CONFIG_SAMSUNG_JACK
char *mic_bias_str=NULL;
char *ext_mic_bias_str = "Headset Mic";
char *int_mic_bias_str = "MIC BIAS2 Power External";
void msm8x16_enable_ear_micbias(bool state)
{
	int nRetVal = 0;
	struct snd_soc_jack *jack = &hs_jack;
	struct snd_soc_codec *codec;
	struct snd_soc_dapm_context *dapm;
	char *str = mic_bias_str;

	printk("%s : str: %s\n", __func__, str);

	if (jack->codec == NULL) { /* audrx_init not yet called */
		pr_err("%s codec==NULL\n", __func__);
		return;
	}
	codec = jack->codec;
	dapm = &codec->dapm;
	mutex_lock(&jack_mutex);

	if (state == 1) {
		nRetVal = snd_soc_dapm_force_enable_pin(dapm, str);
		pr_info("%s enable the codec  pin : %d with state :%d\n"
				, __func__, nRetVal, state);
	} else{
		nRetVal = snd_soc_dapm_disable_pin(dapm, str);
		pr_info("%s disable the codec  pin : %d with state :%d\n"
				, __func__, nRetVal, state);
	}
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL
	jack_connected = state;
#endif
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&jack_mutex);
}
#endif /* CONFIG_SAMSUNG_JACK */

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
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{/* hw:x,3 */
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
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
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
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
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
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
		.name = "MSM8916 HFP TX",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
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
		.name = "QUIN_MI2S Hostless",
		.stream_name = "QUIN_MI2S Hostless",
		.cpu_dai_name = "QUIN_MI2S_RX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
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
	},
	{/* hw:x,27 */
		.name = "MSM8X16 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name	= "MultiMedia10",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	{ /* hw:x,36 */
		.name = "QUAT_MI2S Hostless",
		.stream_name = "QUAT_MI2S Hostless",
		.cpu_dai_name = "QUAT_MI2S_RX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
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
		.be_id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUIN_MI2S_RX,
		.stream_name = "Quinary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.5",
		.platform_name = "msm-pcm-routing",
#if defined(CONFIG_SND_SOC_CS47L15)
		.codec_name = "cs47l15-codec",
		.codec_dai_name = "cs47l15-aif1",
		.dai_fmt = SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF,
		.init = &msm_arizona_init,
#else
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
#endif
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_quin_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
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
		.init = &msm_arizona_init,
#else
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
#endif
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8952_quin_mi2s_be_ops,
		.ignore_suspend = 1,
	},
#if defined(CONFIG_SND_SOC_JACK_AUDIO)
	{
		.name = "MSM8994 JACK LowLatency",
		.stream_name = "MultiMedia17",
		.cpu_dai_name	= "MultiMedia17",
		.platform_name	= "msm-pcm-dsp.1",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA17,
	},
#endif
};

static int msm8952_wsa881x_init(struct snd_soc_dapm_context *dapm)
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
	.name		= "msm8952-cs47l15-snd-card",
	.dai_link	= msm8952_dai,
	.num_links	= ARRAY_SIZE(msm8952_dai),
#if defined(CONFIG_SND_SOC_CS47L15)
	//.dapm_widgets = msm_dapm_widgets,
	//.num_dapm_widgets = ARRAY_SIZE(msm_dapm_widgets),
	//.dapm_routes = msm_cs47l15_routes,
	//.num_dapm_routes = ARRAY_SIZE(msm_cs47l15_routes),
	.late_probe = msm8952_cs47l15_late_probe,
	.set_bias_level = msm_set_bias_level,
	.set_bias_level_post = msm_set_bias_level_post,
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
		switch (q6core_get_avs_version()) {
		case (Q6_SUBSYS_AVS2_6):
		{
			pdata->digital_cdc_clk.clk_val = 0;
			ret = afe_set_digital_codec_core_clock(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
				&pdata->digital_cdc_clk);
				break;
		}
		case (Q6_SUBSYS_AVS2_7):
		{
			pdata->digital_cdc_core_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
				&pdata->digital_cdc_core_clk);
				break;
		}
		case (Q6_SUBSYS_INVALID):
		default:
		{
			ret = -EINVAL;
			pr_err("%s: unknown dsp image\n", __func__);
			break;
		}
		}
		if (ret < 0)
			pr_err("%s failed to disable the MCLK\n", __func__);
		atomic_set(&pdata->mclk_enabled, false);
	}
	mutex_unlock(&pdata->cdc_mclk_mutex);
}


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

	return;
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
				pr_debug("%s: No match found for platform name: %s\n",
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

#if 1
int msm8952_init_wsaswitch_supply(struct platform_device *pdev,
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
#endif

#ifdef CONFIG_SAMSUNG_JACK
static int msm_get_adc(void)
{
	struct snd_soc_jack *jack = &hs_jack;
	struct snd_soc_codec *codec;
	struct msm8916_asoc_mach_data *pdata;
	struct qpnp_vadc_result result;
	struct qpnp_vadc_chip *earjack_vadc;
	int adc_val;
	uint32_t mpp_ch;

	if (jack->codec == NULL) { /* audrx_init not yet called */
		pr_err("%s codec==NULL\n", __func__);
		return -1;
	}
	codec = jack->codec;
	pdata = snd_soc_card_get_drvdata(codec->card);

	mpp_ch = pdata->mpp_ch_scale[0] + P_MUX1_1_3 - 1;

	if (pdata->mpp_ch_scale[2] == 1)
		mpp_ch = pdata->mpp_ch_scale[0] + P_MUX1_1_1 - 1;
	else if (pdata->mpp_ch_scale[2] == 3)
		mpp_ch = pdata->mpp_ch_scale[0] + P_MUX1_1_3 - 1;
	else
		pr_err("%s - invalid channel scale=%d\n",
			__func__, pdata->mpp_ch_scale[2]);

#if defined(CONFIG_SEC_MPP_SHARE)
	sec_mpp_mux_control(EAR_ADC_MUX_SEL_NUM, SEC_MUX_SEL_EAR_ADC, 1);
#endif
	earjack_vadc = qpnp_get_vadc(codec->card->dev, "earjack-read");
	qpnp_vadc_read(earjack_vadc,  mpp_ch, &result);
#if defined(CONFIG_SEC_MPP_SHARE)
	sec_mpp_mux_control(EAR_ADC_MUX_SEL_NUM, SEC_MUX_SEL_EAR_ADC, 0);
#endif

	/* Get voltage in microvolts */
	adc_val = ((int)result.physical)/1000;

	return adc_val;
}
#endif /* CONFIG_SAMSUNG_JACK */


static int msm8952_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm8916_asoc_mach_data *pdata = NULL;
	const char *hs_micbias_type = "qcom,msm-hs-micbias-type";
	const char *ext_pa = "qcom,msm-ext-pa";
	const char *mclk = "qcom,msm-mclk-freq";
	const char *wsa = "asoc-wsa-codec-names";
	const char *wsa_prefix = "asoc-wsa-codec-prefixes";
	const char *type = NULL;
	const char *ext_pa_str = NULL;
	const char *wsa_str = NULL;
	const char *wsa_prefix_str = NULL;
	int num_strings;
	int ret, id, i;
	struct resource	*muxsel;
	char *temp_str = NULL;

	if (ext_codec == true) {
		pr_err("%s: ext codec setprop is true\n", __func__);
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm8916_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate msm8x16_asoc_mach_data\n");
		return -ENOMEM;
	}

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
			"csr_gp_io_mux_quin_ctl");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_quin_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_quin_ctl == NULL) {
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
					dev_err(&pdev->dev,
						"%s:can't allocate wsa codec name\n",
						__func__);
					ret = -ENOMEM;
					goto err;
				}
				msm8952_aux_dev[i].codec_name = temp_str;
				temp_str = NULL;

				temp_str = kstrdup(wsa_str, GFP_KERNEL);
				if (!temp_str) {
					dev_err(&pdev->dev,
						"%s:can't allocate codec dev name\n",
						__func__);
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
					dev_err(&pdev->dev,
						"%s:can't allocate wsa prefix\n",
						__func__);
					ret = -ENOMEM;
					goto err;
				}
				msm8952_codec_conf[i].name_prefix = temp_str;
				temp_str = NULL;
			}

#if 1
			ret = msm8952_init_wsaswitch_supply(pdev, pdata);
			if (ret < 0) {
				pr_err("%s: failed to init wsa_switch vdd supply %d\n",
						__func__, ret);
				goto err;
			}
#endif

			wsa881x_set_mclk_callback(msm8952_enable_wsa_mclk);
		}
	}

	card = &bear_card;
	bear_card.name = dev_name(&pdev->dev);
	card = &bear_card;
	dev_info(&pdev->dev, "cs47l15 default codec configured\n");
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

	ret = is_extspk_gpio_support(pdev, pdata);
	if (ret < 0)
		pr_debug("%s:  doesn't support external speaker pa\n",
				__func__);

	ret = ext_audio_switch_support(pdev, pdata);
	if (ret < 0)
		dev_dbg(&pdev->dev, "%s: doesn't require ext audio switch support\n",
			 __func__);

	ret = of_property_read_string(pdev->dev.of_node,
		hs_micbias_type, &type);
	if (ret) {
		dev_err(&pdev->dev, "%s: missing %s in dt node\n",
			__func__, hs_micbias_type);
		goto err;
	}
	if (!strcmp(type, "external")) {
		dev_dbg(&pdev->dev, "Headset is using external micbias\n");
#ifdef CONFIG_SAMSUNG_JACK
		mic_bias_str = ext_mic_bias_str;
#elif defined(CONFIG_SWITCH_ARIZONA)

#else
		mbhc_cfg.hs_ext_micbias = true;
#endif /* CONFIG_SAMSUNG_JACK */
	} else {
		dev_dbg(&pdev->dev, "Headset is using internal micbias\n");
#ifdef CONFIG_SAMSUNG_JACK
		mic_bias_str = int_mic_bias_str;
#elif defined(CONFIG_SWITCH_ARIZONA)

#else
		mbhc_cfg.hs_ext_micbias = false;
#endif /* CONFIG_SAMSUNG_JACK */
	}

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

	ret = core_get_adsp_ver();

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

	jack_controls.set_micbias = msm8x16_enable_ear_micbias;
	jack_controls.get_adc = msm_get_adc;
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
module_platform_driver(msm8952_asoc_machine_driver);

static ssize_t codec_detect_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int boot = 0;
	if (sscanf(buf, "%du", &boot) == 1) {
		if (boot == 1)
			ext_codec = true;
	}
	return count;
}

static struct kobj_attribute codec_detect_attribute =
	__ATTR(boot, 0220, NULL, codec_detect_store);

static struct attribute *attrs[] = {
	&codec_detect_attribute.attr,
	NULL,
};

static int codec_detector_init_sysfs(void)
{
	int ret = -EINVAL;
	struct kobject *boot_cdc_det = NULL;
	struct attribute_group *attr_group;

	attr_group = kzalloc(sizeof(*(attr_group)),
				GFP_KERNEL);
	if (!attr_group) {
		pr_err("%s: malloc attr_group failed\n",
						__func__);
		ret = -ENOMEM;
		goto error_return;
	}

	attr_group->attrs = attrs;

	boot_cdc_det = kobject_create_and_add("codec_type", kernel_kobj);
	if (!boot_cdc_det) {
		pr_err("%s: sysfs create and add failed\n",
						__func__);
		ret = -ENOMEM;
		goto error_return;
	}

	ret = sysfs_create_group(boot_cdc_det, attr_group);
	if (ret) {
		pr_err("%s: sysfs create group failed %d\n",
							__func__, ret);
		goto error_return;
	}

	return 0;

error_return:
	kfree(attr_group);
	if (boot_cdc_det) {
		kobject_del(boot_cdc_det);
		boot_cdc_det = NULL;
	}

	return ret;
}

static int __init codec_detector_init(void)
{
	int ret = codec_detector_init_sysfs();
	if (ret != 0) {
		pr_err("%s: Error in initing sysfs\n", __func__);
		return ret;
	}
	return 0;
}
module_init(codec_detector_init);


MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8952_asoc_machine_of_match);
