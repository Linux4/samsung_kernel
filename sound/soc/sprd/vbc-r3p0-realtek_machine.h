/*
 * sound/soc/sprd/vbc-r3p0-es755_machine.h
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <uapi/linux/input.h>
#include <sound/vbc-utils.h>
#include <sound/pcm_params.h>
#include "../codecs/rt5659.h"

static const struct snd_kcontrol_new vbc_r3p0_codec_controls[] = {
	BOARD_CODEC_FUNC("Speaker Function", BOARD_FUNC_SPKL),
	BOARD_CODEC_FUNC("Speaker2 Function", BOARD_FUNC_SPKR),
	BOARD_CODEC_FUNC("Earpiece Function", BOARD_FUNC_EAR),
	BOARD_CODEC_FUNC("HeadPhone Function", BOARD_FUNC_HP),
	BOARD_CODEC_FUNC("Line Function", BOARD_FUNC_LINE),
	BOARD_CODEC_FUNC("Mic Function", BOARD_FUNC_MIC),
	BOARD_CODEC_FUNC("Aux Mic Function", BOARD_FUNC_AUXMIC),
	BOARD_CODEC_FUNC("HP Mic Function", BOARD_FUNC_HP_MIC),
	BOARD_CODEC_FUNC("DMic Function", BOARD_FUNC_DMIC),
	BOARD_CODEC_FUNC("DMic1 Function", BOARD_FUNC_DMIC1),
	BOARD_CODEC_FUNC("Digital FM Function", BOARD_FUNC_DFM),

	BOARD_CODEC_MUTE("Speaker Mute", BOARD_FUNC_SPKL),
	BOARD_CODEC_MUTE("Speaker2 Mute", BOARD_FUNC_SPKR),
	BOARD_CODEC_MUTE("Earpiece Mute", BOARD_FUNC_EAR),
	BOARD_CODEC_MUTE("HeadPhone Mute", BOARD_FUNC_HP),
};

/* board supported audio map */
static const struct snd_soc_dapm_route vbc_r3p0_codec_map[] = {
	{"HeadPhone Jack", NULL, "HEAD_P_L"},
	{"HeadPhone Jack", NULL, "HEAD_P_R"},
	{"Ext Spk", NULL, "AOL"},
	{"Ext Spk2", NULL, "AOR"},
	{"Ext Ear", NULL, "EAR"},
	{"MIC", NULL, "Mic Jack"},
	{"AUXMIC", NULL, "Aux Mic Jack"},
	{"HPMIC", NULL, "HP Mic Jack"},
	{"AIL", NULL, "Line Jack"},
	{"AIR", NULL, "Line Jack"},
	{"DMIC", NULL, "DMic Jack"},
	{"DMIC1", NULL, "DMic1 Jack"},

	{"inter HP PA", NULL, "HP PA"},
	{"inter Spk PA", NULL, "Spk PA"},

	/* VBC -- SPRD-CODEC */
	{"Aud input", NULL, "AD Clk"},
	{"Aud1 input", NULL, "AD Clk"},

	{"Aud input", NULL, "Digital ADCL Switch"},
	{"Aud input", NULL, "Digital ADCR Switch"},

	{"Aud1 input", NULL, "Digital ADC1L Switch"},
	{"Aud1 input", NULL, "Digital ADC1R Switch"},

};
#if 0
extern struct sprd_dfm_priv dfm;
static int dfm_rate(struct snd_pcm_hw_params *params)
{
	dfm.hw_rate = params_rate(params);

	if (dfm.hw_rate == 8000)
		dfm.sample_rate = 8000;
	else {
#ifdef CONFIG_SND_SOC_SPRD_VBC_SRC_OPEN
		dfm.sample_rate = 44100;
#else
		dfm.sample_rate = params_rate(params);
#endif
	}
	sp_asoc_pr_dbg("%s: hw_rate:%d,  sample_rate:%d \n",
		       __func__, dfm.hw_rate, dfm.sample_rate);

	if (dfm.hw_rate != dfm.sample_rate) {
		struct snd_interval *rate =
		    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
		struct snd_interval dfm_rates;
		dfm_rates.min = dfm.sample_rate;
		dfm_rates.max = dfm.sample_rate;
		dfm_rates.openmin = dfm_rates.openmax = 0;
		dfm_rates.integer = 0;

		return snd_interval_refine(rate, &dfm_rates);
	}
	return 0;
}

static int dfm_params(struct snd_pcm_substream *substream,
		      struct snd_pcm_hw_params *params)
{

	sp_asoc_pr_dbg("%s \n", __func__);
	dfm_rate(params);
	return 0;
}

static const struct snd_soc_ops dfm_ops = {
	.hw_params = dfm_params,
};

#endif
static int sprd_rt5658_hw_params(struct snd_pcm_substream *substream,
                    struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct snd_soc_dai *codec_dai = rtd->codec_dai;
    struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
    int ret = 0;
    int bclk_div,rclk_div;
    unsigned long rate = params_rate(params);
    int channels = params_channels(params);
    snd_pcm_format_t fmt= params_format(params);

    unsigned int RCLKSRC, RCLK, pre_div;
	//sample rate * 256fs
	RCLK = 48000 * 256;
	
	pr_info("rate=%d, channels=%d, fmt=%d\n", rate, channels, fmt);
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
	SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	#if 0
	// set cpu clock 26M
	// set mclk 26M chipram chipram/arch/arm/cpu/armv7/sc9630/mcu.c 
	//0x402e0018[1]=1; CLK26M_SINE_O out square wave
	ret = snd_soc_dai_set_sysclk( cpu_dai, 0, 0, 0);
	#endif
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5659_SCLK_S_PLL1, RCLK, SND_SOC_CLOCK_IN);
	if (ret < 0){
		pr_err("snd_soc_dai_set_sysclk failed ret=%d\n", ret);
		return ret;
	}
	
	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5659_PLL1_S_MCLK, 26000000, RCLK);
	if (ret < 0){
		pr_err("snd_soc_dai_set_pll failed ret = %d \n", ret);
		return ret; 
	}

    return 0;
}

static struct snd_soc_ops sprd_rt5658_ops = {
   
    .hw_params = sprd_rt5658_hw_params,
};

static struct snd_soc_dai_link vbc_r3p0_codec_dai[] = {
/*
 * Frontend DAIs - i.e. userspace visible interfaces (ALSA PCMs)
 */
	{
		.name = "(vbc)Prototype earSmart I2S PortA",
		.stream_name = "I2S-PCM PortA",
		.cpu_dai_name = "normal-without-dsp",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "rt5659-aif1",
		.codec_name = "rt5659.4-001b",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
	{
		.name = "(vbc)Prototype earSmart Voice PortA",
		.stream_name = "I2S-PCM-Voice PortA",
		.cpu_dai_name = "voice",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "rt5659-aif1",
		.codec_name = "rt5659.4-001b",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
	{
		.name = "(vbc)Prototype earSmart I2S PortB",
		.stream_name = "I2S-PCM PortB",
		.cpu_dai_name = "normal-without-dsp",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "rt5659-aif1",
		.codec_name = "rt5659.4-001b",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
	{
		.name = "(vbc)Prototype earSmart Voice Capture PortA",
		.stream_name = "I2S-PCM-Voice-Capture PortA",
		.cpu_dai_name = "voice-capture",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "rt5659-aif1",
		.codec_name = "rt5659.4-001b",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
	{
		.name = "(vbc)Prototype earSmart fm PortA",
		.stream_name = "I2S-PCM-FM PortA",
		.cpu_dai_name = "fm",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "rt5659-aif1",
		.codec_name = "rt5659.4-001b",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
	{
		.name = "sprd offload virtual",
		.stream_name = "offload stream",
		.cpu_dai_name = "offload",
		/*.platform_name = "sprd-pcm-audio",*/
		.codec_name = "rt5659.4-001b",
		.codec_dai_name = "rt5659-aif1",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
#if 0
	/*
	* loop back record 6
	*/
	{
		.name = "(vbc)Prototype earSmart loop record PortA",
		.stream_name = "I2S-PCM loop record PortA",
		.cpu_dai_name = "loop_record",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "rt5659-aif1",
		.codec_name = "rt5659.4-001b",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
	/*
	* loop back play 7
	*/
	{
		.name = "(vbc)Prototype earSmart loop play PortA",
		.stream_name = "I2S-PCM loop play PortA",
		.cpu_dai_name = "loop_play",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "rt5659-aif1",
		.codec_name = "rt5659.4-001b",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &sprd_rt5658_ops,
	},
#endif
#if 0
	/*
	codec portb perm
	*/
	{
		.name = "earSmart PortB",
		.stream_name = "I2S-PCM PortB",
		.cpu_dai_name = "null-cpu",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "earSmart-portb",
		.codec_name = "earSmart_i2c.5-003e",
		.init = &escore_soc_audio_init,
		.ops = &escore_machine_audio_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
#endif
#if 0
	{
		.name = "(vbc)Prototype earSmart btcall PortA",
		.stream_name = "I2S-PCM PortA",
		.cpu_dai_name = "normal-without-dsp",
		.platform_name = "sprd-pcm-audio",
		.codec_dai_name = "earSmart-porta",
		.codec_name = "earSmart_i2c.5-003e",
		.init = &escore_soc_audio_init,
		.ops = &escore_machine_audio_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
#endif
#if 0
	{
		.name = "sprdcompress",
		.stream_name = "sprd-compr",

		.codec_name = "rt5659.4-001b",
		.platform_name = "sprd-compr-platform",
		.cpu_dai_name = "compress-dai",
		.codec_dai_name = "null-codec",
	},
#endif
};

static struct snd_soc_card vbc_r3p0_codec_card = {
	.dai_link = vbc_r3p0_codec_dai,
	.num_links = ARRAY_SIZE(vbc_r3p0_codec_dai),
	.owner = THIS_MODULE,

	//.controls = vbc_r3p0_codec_controls,
	//.num_controls = ARRAY_SIZE(vbc_r3p0_codec_controls),
	//.dapm_widgets = sprd_codec_dapm_widgets,
	//.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
	//.dapm_routes = vbc_r3p0_codec_map,
	//.num_dapm_routes = ARRAY_SIZE(vbc_r3p0_codec_map),
	//.late_probe = board_late_probe,
};

static int vbc_r3p0_codec_probe(struct platform_device *pdev)
{
	return sprd_asoc_probe(pdev, &vbc_r3p0_codec_card);
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_r3p0_codec_of_match[] = {
	{.compatible = "sprd,vbc-r3p0-sprd-codec-realtek",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_r3p0_codec_of_match);
#endif

static struct platform_driver vbc_r3p0_codec_driver = {
	.driver = {
		   .name = "vbc-r3p0-realtek_machine",
		   .owner = THIS_MODULE,
		   .pm = &snd_soc_pm_ops,
		   .of_match_table = of_match_ptr(vbc_r3p0_codec_of_match),
		   },
	.probe = vbc_r3p0_codec_probe,
	.remove = sprd_asoc_remove,
	.shutdown = sprd_asoc_shutdown,
};

static int __init vbc_r3p0_codec_init(void)
{
	return platform_driver_register(&vbc_r3p0_codec_driver);
}

late_initcall_sync(vbc_r3p0_codec_init);

#include <linux/of_platform.h>

//jian.chen temporary modification
//#if (defined(CONFIG_ARCH_SCX35L64)||defined(CONFIG_ARCH_SCX35LT8))
const struct of_device_id of_sprd_late_bus_match_table[] = {
	{ .compatible = "sprd,sound", },
	{}
};
static void __init sc8830_init_late(void)
{
	of_platform_populate(of_find_node_by_path("/sprd-audio-devices"),
				of_sprd_late_bus_match_table, NULL, NULL);
}
core_initcall(sc8830_init_late);
//#endif

