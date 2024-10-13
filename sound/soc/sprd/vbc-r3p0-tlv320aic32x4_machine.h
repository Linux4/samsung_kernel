/*
 * sound/soc/sprd/vbc-r3p0-sprd-codec-ti.h
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
#if 0
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
static struct snd_soc_dai_link vbc_r3p0_codec_dai[] = {
	{
	 .name = "vbc(r3)-codec(TI)-ap-outdsp",
	 .stream_name = "HiFi",

	 .codec_name = "tlv320aic32x4.0-0018",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "normal-without-dsp",
	 .codec_dai_name = "tlv320aic32x4-hifi",
	 .ignore_suspend = 1,
	 },
	 {
	 .name = "vbc(r3)-codec(TI)-voice",
	 .stream_name = "voice",

	 .codec_name = "tlv320aic32x4.0-0018",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "voice",
	 .codec_dai_name = "tlv320aic32x4-hifi",
	 .ignore_suspend = 1,
	 },
	 {
	 .name = "vbc(r3)-codec(TI)-fm",
	 .stream_name = "fm",

	 .codec_name = "tlv320aic32x4.0-0018",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "fm",
	 .codec_dai_name = "tlv320aic32x4-hifi",
	 .ignore_suspend = 1,
	 },
	 {
	 .name = "vbc(r3)-codec(TI)-offload",
	 .stream_name = "offload",

	 .codec_name = "tlv320aic32x4.0-0018",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "offload",
	 .codec_dai_name = "tlv320aic32x4-hifi",
	 .ignore_suspend = 1,
	 },
	 {
	 .name = "vbc(r3)-codec(TI)-fm-c-withdsp",
	 .stream_name = "fm-c-withdsp",

	 .codec_name = "tlv320aic32x4.0-0018",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "fm-c-withdsp",
	 .codec_dai_name = "tlv320aic32x4-hifi",
	 .ignore_suspend = 1,
	 },
};

static struct snd_soc_card vbc_r3p0_codec_card = {
	.name = "sprdphone",
	.dai_link = vbc_r3p0_codec_dai,
	.num_links = ARRAY_SIZE(vbc_r3p0_codec_dai),
	.owner = THIS_MODULE,

	//.controls = vbc_r3p0_codec_controls,
	//.num_controls = ARRAY_SIZE(vbc_r3p0_codec_controls),
	//.dapm_widgets = sprd_codec_dapm_widgets,
	//.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
	//.dapm_routes = vbc_r3p0_codec_map,
	//.num_dapm_routes = ARRAY_SIZE(vbc_r3p0_codec_map),
	.late_probe = board_late_probe,
};

static int vbc_r3p0_codec_probe(struct platform_device *pdev)
{
	return sprd_asoc_probe(pdev, &vbc_r3p0_codec_card);
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_r3p0_codec_of_match[] = {
	{.compatible = "sprd,vbc-r3p0-tlv320aic32x4_machine",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_r3p0_codec_of_match);
#endif

static struct platform_driver vbc_r3p0_codec_driver = {
	.driver = {
		   .name = "vbc-r3p0-tlv320aic32x4_machine",
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

