/*
 * sound/soc/sprd/vbc-r1p0-sprd-codec-v1.h
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

static const struct snd_kcontrol_new vbc_r1p0_codec_v1_controls[] = {
	BOARD_CODEC_FUNC("Speaker Function", BOARD_FUNC_SPKL),
	BOARD_CODEC_FUNC("Speaker2 Function", BOARD_FUNC_SPKR),
	BOARD_CODEC_FUNC("Earpiece Function", BOARD_FUNC_EAR),
	BOARD_CODEC_FUNC("HeadPhone Function", BOARD_FUNC_HP),
	BOARD_CODEC_FUNC("Line Function", BOARD_FUNC_LINE),
	BOARD_CODEC_FUNC("Mic Function", BOARD_FUNC_MIC),
	BOARD_CODEC_FUNC("Aux Mic Function", BOARD_FUNC_AUXMIC),
	BOARD_CODEC_FUNC("HP Mic Function", BOARD_FUNC_HP_MIC),
	BOARD_CODEC_FUNC("Digital FM Function", BOARD_FUNC_DFM),

	BOARD_CODEC_MUTE("Speaker Mute", BOARD_FUNC_SPKL),
	BOARD_CODEC_MUTE("Speaker2 Mute", BOARD_FUNC_SPKR),
	BOARD_CODEC_MUTE("Earpiece Mute", BOARD_FUNC_EAR),
	BOARD_CODEC_MUTE("HeadPhone Mute", BOARD_FUNC_HP),
};

/* board supported audio map */
static const struct snd_soc_dapm_route vbc_r1p0_codec_v1_map[] = {
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

	{"inter Spk PA", NULL, "Spk PA"},

	/* VBC -- SPRD-CODEC */
	{"Aud input", NULL, "AD Clk"},

	{"Aud input", NULL, "Digital ADCL Switch"},
	{"Aud input", NULL, "Digital ADCR Switch"},

};

static struct snd_soc_dai_link vbc_r1p0_codec_v1_dai[] = {
	{
	 .name = "vbc(r1)-codec(v1)-ap",
	 .stream_name = "HiFi",

	 .codec_name = "sprd-codec-v1",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vbc-r1p0",
	 .codec_dai_name = "sprd-codec-v1-i2s",
	 },
#ifdef CONFIG_SND_SOC_SPRD_VAUDIO
	{
	 .name = "vbc(r1)-codec(v1)-dsp",
	 .stream_name = "Voice",

	 .codec_name = "sprd-codec-v1",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vaudio",
	 .codec_dai_name = "sprd-codec-v1-vaudio",
	 },
#endif
	{
	 .name = "vbc(r1)-dfm",
	 .stream_name = "Dfm",

	 .codec_name = "sprd-codec-v1",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "vbc-dfm",
	 .codec_dai_name = "sprd-codec-v1-fm",
	 },
};

static struct snd_soc_card vbc_r1p0_codec_v1_card = {
	.name = "sprdphone",
	.dai_link = vbc_r1p0_codec_v1_dai,
	.num_links = ARRAY_SIZE(vbc_r1p0_codec_v1_dai),
	.owner = THIS_MODULE,

	.controls = vbc_r1p0_codec_v1_controls,
	.num_controls = ARRAY_SIZE(vbc_r1p0_codec_v1_controls),
	.dapm_widgets = sprd_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
	.dapm_routes = vbc_r1p0_codec_v1_map,
	.num_dapm_routes = ARRAY_SIZE(vbc_r1p0_codec_v1_map),
	.late_probe = board_late_probe,
};

static int vbc_r1p0_codec_v1_probe(struct platform_device *pdev)
{
	return sprd_asoc_probe(pdev, &vbc_r1p0_codec_v1_card);
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_r1p0_codec_v1_of_match[] = {
	{.compatible = "sprd,vbc-r1p0-sprd-codec-v1",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_r1p0_codec_v1_of_match);
#endif

static struct platform_driver vbc_r1p0_codec_v1_driver = {
	.driver = {
		   .name = "vbc-r1p0-sprd-codec-v1",
		   .owner = THIS_MODULE,
		   .pm = &snd_soc_pm_ops,
		   .of_match_table = of_match_ptr(vbc_r1p0_codec_v1_of_match),
		   },
	.probe = vbc_r1p0_codec_v1_probe,
	.remove = sprd_asoc_remove,
	.shutdown = sprd_asoc_shutdown,
};

module_platform_driver(vbc_r1p0_codec_v1_driver);
