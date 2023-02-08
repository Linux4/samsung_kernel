/*
 *  espresso3250_wm5110.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/firmware.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/list.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include <mach/regs-pmu.h>

#include "i2s.h"
#include "i2s-regs.h"

static bool clkout_enabled;

static void espresso_enable_mclk(bool on)
{
	pr_debug("%s: %s\n", __func__, on ? "on" : "off");

	clkout_enabled = on;
	/* XUSBXTI 24MHz via XCLKOUT */
	writel(on ? 0x0900 : 0x0901, EXYNOS_PMU_DEBUG);
}

static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
	pr_debug("%s: EPLL rate = %ld\n",
		__func__, clk_get_rate(fout_epll));
out:
	clk_put(fout_epll);

	return 0;
}

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
/*
 * ESPRESSO I2S DAI operations. (AP Master)
 */
static int espresso_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, sclk, bfs, psr, rfs, ret;
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
		pr_err("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	pll = sclk;
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
#else
/*
 * ESPRESSO I2S DAI operations. (AP Master)
 */
static int espresso_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	pr_err("Not yet supported!\n");
	return -EINVAL;
}
#endif

static struct snd_soc_ops espresso_ops = {
	.hw_params = espresso_hw_params,
};

static struct snd_soc_dai_link espresso_dai[] = {
	{
		.name = "WM5110 PRI",
		.stream_name = "i2s",
		.cpu_dai_name = "samsung-i2s.2",
		.codec_dai_name = "dummy-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "dummy-codec",
		.ops = &espresso_ops,
	}
};

static int espresso_suspend_post(struct snd_soc_card *card)
{
	espresso_enable_mclk(false);
	return 0;
}

static int espresso_resume_pre(struct snd_soc_card *card)
{
	espresso_enable_mclk(true);
	return 0;
}

static struct snd_soc_card espresso = {
	.name = "ESPRESSO-I2S",
	.owner = THIS_MODULE,
	.suspend_post = espresso_suspend_post,
	.resume_pre = espresso_resume_pre,
	.dai_link = espresso_dai,
	.num_links = ARRAY_SIZE(espresso_dai),
};

static struct platform_device *espresso_snd_device;

static int __init espresso_audio_init(void)
{
	int ret;

	espresso_snd_device = platform_device_alloc("soc-audio", -1);
	if (!espresso_snd_device)
		return -ENOMEM;

	platform_set_drvdata(espresso_snd_device, &espresso);

	ret = platform_device_add(espresso_snd_device);
	if (ret)
		platform_device_put(espresso_snd_device);

	return ret;
}
module_init(espresso_audio_init);

static void __exit espresso_audio_exit(void)
{
	platform_device_unregister(espresso_snd_device);
}
module_exit(espresso_audio_exit);

MODULE_DESCRIPTION("ALSA SoC ESPRESSO WM5110");
MODULE_LICENSE("GPL");
