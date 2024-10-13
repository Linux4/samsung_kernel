/*
 * sound/soc/sprd/vbc-r2p0-sprd-codec-v4.h
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

static const struct snd_kcontrol_new vbc_r2p0_codec_controls[] = {
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
static const struct snd_soc_dapm_route vbc_r2p0_codec_map[] = {
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

static struct snd_soc_dai_link vbc_r2p0_codec_dai[] = {
	{
	 .name = "vbc(r2)-codec(v4)-ap",
	 .stream_name = "HiFi",

	 .codec_name = "sprd-codec",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vbc-r2p0",
	 .codec_dai_name = "sprd-codec-i2s",
	 },
#ifdef CONFIG_SND_SOC_SPRD_VAUDIO
	{
	 .name = "vbc(r2)-codec(v4)-dsp",
	 .stream_name = "Voice",

	 .codec_name = "sprd-codec",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vaudio",
	 .codec_dai_name = "sprd-codec-vaudio",
	 },
#endif
	{
	 .name = "aux-captrue",
	 .stream_name = "AuxRecord",

	 .codec_name = "sprd-codec",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vbc-r2p0-ad23",
	 .codec_dai_name = "codec-i2s-ext",
	 },
#ifdef CONFIG_SND_SOC_SPRD_VAUDIO
	{
	 .name = "aux-dsp-captrue",
	 .stream_name = "AuxDSPCaptrue",

	 .codec_name = "sprd-codec",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vaudio-ad23",
	 .codec_dai_name = "codec-vaudio-ext",
	 },
#endif
	{
	 .name = "vbc(r2)-dfm",
	 .stream_name = "Dfm",

	 .codec_name = "sprd-codec",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vbc-dfm",
	 .codec_dai_name = "sprd-codec-fm",
	 .ops = &dfm_ops,
	 },
};

static struct snd_soc_card vbc_r2p0_codec_card = {
	.name = "sprdphone",
	.dai_link = vbc_r2p0_codec_dai,
	.num_links = ARRAY_SIZE(vbc_r2p0_codec_dai),
	.owner = THIS_MODULE,

	.controls = vbc_r2p0_codec_controls,
	.num_controls = ARRAY_SIZE(vbc_r2p0_codec_controls),
	.dapm_widgets = sprd_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
	.dapm_routes = vbc_r2p0_codec_map,
	.num_dapm_routes = ARRAY_SIZE(vbc_r2p0_codec_map),
	.late_probe = board_late_probe,
};

static int vbc_r2p0_codec_probe(struct platform_device *pdev)
{
	return sprd_asoc_probe(pdev, &vbc_r2p0_codec_card);
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_r2p0_codec_of_match[] = {
	{.compatible = "sprd,vbc-r2p0-sprd-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_r2p0_codec_of_match);
#endif

static struct platform_driver vbc_r2p0_codec_driver = {
	.driver = {
		   .name = "vbc-r2p0-sprd-codec",
		   .owner = THIS_MODULE,
		   .pm = &snd_soc_pm_ops,
		   .of_match_table = of_match_ptr(vbc_r2p0_codec_of_match),
		   },
	.probe = vbc_r2p0_codec_probe,
	.remove = sprd_asoc_remove,
	.shutdown = sprd_asoc_shutdown,
};

module_platform_driver(vbc_r2p0_codec_driver);
