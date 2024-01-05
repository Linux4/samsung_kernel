/*
 *  xyref5260_es515.c
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
#include "spdif.h"

#define XYREF_AUD_PLL_FREQ	(196608000)

static bool clkout_enabled;

static void xyref_enable_mclk(bool on)
{
	pr_debug("%s: %s\n", __func__, on ? "on" : "off");

	clkout_enabled = on;
	writel(on ? 0x1000 : 0x1001, EXYNOS_PMU_DEBUG);
}

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

static int xyref_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	/* CLKOUT(XXTI) for eS515 MCLK */
	xyref_enable_mclk(true);

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	return 0;
}

#ifdef CONFIG_SND_SAMSUNG_AUX_HDMI
/*
 * XYREF HDMI I2S DAI operations.
 */
static int xyref_hdmi_hw_params(struct snd_pcm_substream *substream,
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
		if (sclk * div > XYREF_AUD_PLL_FREQ)
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

static struct snd_soc_ops xyref_hdmi_ops = {
	.hw_params = xyref_hdmi_hw_params,
};
#endif

#ifdef CONFIG_SND_SAMSUNG_AUX_SPDIF
/*
 * XYREF S/PDIF DAI operations. (AP master)
 */
static int xyref_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct clk *sclk_peri, *sclk_spdif;
	unsigned long rclk;
	int ret, ratio, pll, div, sclk;

	sclk_peri = clk_get(NULL, "sclk_peri_aud");
	if (IS_ERR(sclk_peri)) {
		pr_err("failed to get sclk_peri_aud clock\n");
		return -EINVAL;
	}

	sclk_spdif = clk_get(NULL, "sclk_spdif");
	if (IS_ERR(sclk_spdif)) {
		pr_err("failed to get sclk_spdif clock\n");
		return -EINVAL;
	}

	clk_set_parent(sclk_spdif, sclk_peri);
	clk_put(sclk_peri);
	clk_put(sclk_spdif);

	switch (params_rate(params)) {
	case 48000:
	case 96000:
		break;
	default:
		return -EINVAL;
	}

	/* Setting ratio to 512fs helps to use S/PDIF with HDMI without
	 * modify S/PDIF ASoC machine driver.
	 */
	ratio = 512;
	rclk = params_rate(params) * ratio;

	/* Set AUD_PLL frequency */
	sclk = rclk;
	for (div = 2; div <= 16; div++) {
		if (sclk * div > XYREF_AUD_PLL_FREQ)
			break;
	}
	pll = sclk * (div - 1);
	set_aud_pll_rate(pll);

	/* Set S/PDIF uses internal source clock */
	ret = snd_soc_dai_set_sysclk(cpu_dai, SND_SOC_SPDIF_INT_MCLK,
					rclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	return ret;
}

static struct snd_soc_ops xyref_spdif_ops = {
	.hw_params = xyref_spdif_hw_params,
};
#endif

/*
 * XYREF eS515 I2S DAI operations. (Codec master)
 */
static struct snd_soc_ops xyref_ops = {
	.hw_params = xyref_hw_params,
};

static struct snd_soc_dai_link xyref_dai[] = {
	{ /* Primary DAI i/f */
		.name = "ES515 PRI",
		.stream_name = "i2s0-pri",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "es515-porta",
		.platform_name = "samsung-audio",
		.codec_name = "es515-codec.1-003e",
		.ops = &xyref_ops,
	}, { /* Secondary DAI i/f */
		.name = "ES515 SEC",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "es515-porta",
		.platform_name = "samsung-audio",
		.codec_name = "es515-codec.1-003e",
		.ops = &xyref_ops,
#ifdef CONFIG_SND_SAMSUNG_AUX_HDMI
	}, { /* Aux DAI i/f */
		.name = "HDMI",
		.stream_name = "i2s1",
		.cpu_dai_name = "samsung-i2s.1",
		.codec_dai_name = "dummy-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "dummy-codec",
		.ops = &xyref_hdmi_ops,
#endif
#ifdef CONFIG_SND_SAMSUNG_AUX_SPDIF
	}, { /* Aux DAI i/f */
		.name = "S/PDIF",
		.stream_name = "spdif",
		.cpu_dai_name = "samsung-spdif",
		.codec_dai_name = "dummy-aif2",
		.platform_name = "samsung-audio",
		.codec_name = "dummy-codec",
		.ops = &xyref_spdif_ops,
#endif
	}
};

static int xyref_suspend_post(struct snd_soc_card *card)
{
	return 0;
}

static int xyref_resume_pre(struct snd_soc_card *card)
{
	return 0;
}

static struct snd_soc_card xyref = {
	.name = "XYREF-I2S",
	.owner = THIS_MODULE,
	.suspend_post = xyref_suspend_post,
	.resume_pre = xyref_resume_pre,
	.dai_link = xyref_dai,
	.num_links = ARRAY_SIZE(xyref_dai),
};

static struct platform_device *xyref_snd_device;

static int __init xyref_audio_init(void)
{
	int ret;

	xyref_snd_device = platform_device_alloc("soc-audio", -1);
	if (!xyref_snd_device)
		return -ENOMEM;

	platform_set_drvdata(xyref_snd_device, &xyref);

	/* CLKOUT(XXTI) for eS515 MCLK */
	xyref_enable_mclk(true);

	ret = platform_device_add(xyref_snd_device);
	if (ret)
		platform_device_put(xyref_snd_device);

	return ret;
}
module_init(xyref_audio_init);

static void __exit xyref_audio_exit(void)
{
	platform_device_unregister(xyref_snd_device);
}
module_exit(xyref_audio_exit);

MODULE_DESCRIPTION("ALSA SoC XYREF eS515");
MODULE_LICENSE("GPL");
