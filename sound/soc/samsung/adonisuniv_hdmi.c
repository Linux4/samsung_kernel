/*
 *  adonisuniv_hdmi.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include <mach/regs-pmu.h>

#include "i2s.h"
#include "i2s-regs.h"

/* AdonisUniv use CLKOUT from AP */
#define ADONISUNIV_AUD_PLL_FREQ	196608000

static struct snd_soc_card adonisuniv_hdmi_card;

static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	rate += 20;		/* margin */
	clk_set_rate(fout_epll, rate);
	pr_debug("%s: EPLL rate = %ld\n",
		__func__, clk_get_rate(fout_epll));
out:
	clk_put(fout_epll);

	return 0;
}

static int adonisuniv_hdmi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, div, sclk, bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 48000:
	case 96000:
	case 192000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 12288000:
	case 18432000:
		psr = 4;
		break;
	case 24576000:
	case 36864000:
		psr = 2;
		break;
	case 49152000:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	for (div = 2; div <= 16; div++) {
		if (sclk * div > ADONISUNIV_AUD_PLL_FREQ)
			break;
	}
	pll = sclk * (div - 1);
	set_aud_pll_rate(pll);

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops adonisuniv_hdmi_ops = {
	.hw_params = adonisuniv_hdmi_hw_params,
};

static struct snd_soc_dai_link adonisuniv_hdmi_dai[] = {
	{ /* Aux DAI i/f */
		.name = "HDMI",
		.stream_name = "i2s1",
		.cpu_dai_name = "samsung-i2s.1",
		.codec_dai_name = "dummy-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "dummy-codec",
		.ops = &adonisuniv_hdmi_ops,
	},
};

static struct snd_soc_card adonisuniv_hdmi_card = {
	.name = "AdonisUniv-HDMI",
	.owner = THIS_MODULE,
	.dai_link = adonisuniv_hdmi_dai,
	.num_links = ARRAY_SIZE(adonisuniv_hdmi_dai),
};

static int adonisuniv_hdmi_probe(struct platform_device *pdev)
{
	int ret;

	adonisuniv_hdmi_card.dev = &pdev->dev;
	ret = snd_soc_register_card(&adonisuniv_hdmi_card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
		return ret;
	}

	return 0;
}

static int adonisuniv_hdmi_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&adonisuniv_hdmi_card);

	return 0;
}

static struct platform_driver adonisuniv_hdmi_driver = {
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "adonisuniv-hdmi",
	},
	.probe	= adonisuniv_hdmi_probe,
	.remove	= __devexit_p(adonisuniv_hdmi_remove),
};

module_platform_driver(adonisuniv_hdmi_driver);

MODULE_DESCRIPTION("ALSA SoC AdonisUniv HDMI");
MODULE_LICENSE("GPL");