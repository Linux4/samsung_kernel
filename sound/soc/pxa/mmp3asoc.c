/*
 * linux/sound/soc/pxa/mmp3asoc.c
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <linux/io.h>
#include <linux/uaccess.h>
#include <mach/regs-audio.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>

#include "../codecs/88pm805-codec.h"
#include "mmp-sspa.h"

#define DSP_AUDIO_CONFIG_REG		(AUD_VIRT_BASE2 + 0x00)
#define DSA_CORE_CLK_RES_CTRL		(AUD_VIRT_BASE2 + 0x28)
#define DSA_SSP_CLK_RES_CTRL		(AUD_VIRT_BASE2 + 0x30)
#define DSA_ABU_CLK_RES_CTRL		(AUD_VIRT_BASE2 + 0x34)

#define MMP3ASOC_SAMPLE_RATES SNDRV_PCM_RATE_44100

/* PM_SLEEP: suspend/resume purpose */
static uint32_t aud_ctrl;
static uint32_t aud_pll_ctrl0;
static uint32_t aud_pll_ctrl1;
static struct clk *audio_clk;

static int mmp3asoc_elba_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	cpu_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->playback.rates = MMP3ASOC_SAMPLE_RATES;
	cpu_dai->driver->capture.rates = MMP3ASOC_SAMPLE_RATES;

	return 0;
}

static int mmp3asoc_elba_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;

	pr_debug("%s: enter, rate %d\n", __func__, params_rate(params));

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = params_rate(params) * 128;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = params_rate(params) * 512;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

#ifdef CONFIG_SND_ELBA_MASTER_MODE
	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
#else
	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
#endif

	snd_soc_dai_set_sysclk(cpu_dai, MMP_SSPA_CLK_AUDIO, freq_out, 0);
	snd_soc_dai_set_pll(cpu_dai, MMP_SYSCLK, 0, freq_out, sysclk);
	snd_soc_dai_set_pll(cpu_dai, MMP_SSPA_CLK, 0, freq_out, sspa_mclk);

	/* set elba sysclk */
	snd_soc_dai_set_sysclk(codec_dai, 0, 0, PM805_CODEC_CLK_DIR_OUT);

	return 0;
}

#ifdef CONFIG_PM
static int mmp3asoc_suspend_post(struct snd_soc_card *card)
{
	aud_ctrl = __raw_readl(AUD_CTL);
	aud_pll_ctrl0 = __raw_readl(AUD_PLL_CTL0);
	aud_pll_ctrl1 = __raw_readl(AUD_PLL_CTL1);

	clk_disable(audio_clk);
	return 0;
}

static int mmp3asoc_resume_pre(struct snd_soc_card *card)
{
	clk_enable(audio_clk);

	__raw_writel(aud_ctrl, AUD_CTL);
	__raw_writel(aud_pll_ctrl0, AUD_PLL_CTL0);
	__raw_writel(aud_pll_ctrl1, AUD_PLL_CTL1);

	return 0;
}
#endif

static void audio_subsystem_poweron(void)
{
	unsigned int ret;
	pr_debug(" audio subsystem power on (A stepping) 0x%08x\n",
	       __raw_readl(APMU_REG(0x220)));

	/* enable power switch 01 */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret |= (0x1 << 9);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);

	/* enable power switch 11 */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret |= (0x1 << 10);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);

	/* enable audio memory redundancy start */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret |= (0x1 << 2);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);
	udelay(100);

	/* enable SRAM power */
	ret = __raw_readl(APMU_AUDIO_SRAM_PWR);
	ret |= 0x5;
	__raw_writel(ret, APMU_AUDIO_SRAM_PWR);

	ret = __raw_readl(APMU_AUDIO_SRAM_PWR);
	ret |= 0xa;
	__raw_writel(ret, APMU_AUDIO_SRAM_PWR);

	ret = __raw_readl(APMU_AUDIO_SRAM_PWR);
	ret |= 0xc0;
	__raw_writel(ret, APMU_AUDIO_SRAM_PWR);

	/* audio island */
	ret = __raw_readl(APMU_ISLD_DSPA_CTRL);
	ret &= ~(0x7);
	__raw_writel(ret, APMU_ISLD_DSPA_CTRL);

	ret = __raw_readl(APMU_ISLD_DSPA_CTRL);
	ret |= (0x1 << 4);
	__raw_writel(ret, APMU_ISLD_DSPA_CTRL);
	udelay(100);

	ret = __raw_readl(APMU_ISLD_DSPA_CTRL);
	ret &= ~(0x1 << 4);
	__raw_writel(ret, APMU_ISLD_DSPA_CTRL);

	/* audio DSA */
	ret = __raw_readl(APMU_AUDIO_DSA);
	ret &= ~(0xf);
	ret |= 0xa;
	__raw_writel(ret, APMU_AUDIO_DSA);
	udelay(100);

	ret = __raw_readl(APMU_AUDIO_DSA);
	ret |= 0xf;
	__raw_writel(ret, APMU_AUDIO_DSA);
	udelay(100);

	/* SSPA1 BIT/SYSCLK */
	__raw_writel(0xd3ee2276, MPMU_ISCCRX0);
	__raw_writel(0xd0040040, MPMU_ISCCRX1);

	/* disable isolation */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret |= (0x1 << 8);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);

	/* enable peripheral */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret |= (0x1 << 4);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);

	/* pull peripheral out of reset */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret |= (0x1 << 1);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);
	udelay(100);

	/* Audio CFG: DSP core will stall after release from reset */
	ret = __raw_readl(DSP_AUDIO_CONFIG_REG);
	ret &= ~(0x1 << 1);
	__raw_writel(ret, DSP_AUDIO_CONFIG_REG);

	/* DSP core clock : enable clock divier  */
	ret = __raw_readl(DSP_AUDIO_CONFIG_REG);
	ret |= (0x1 << 3);
	__raw_writel(ret, DSP_AUDIO_CONFIG_REG);

	/* Release the core reset */
	ret = __raw_readl(DSP_AUDIO_CONFIG_REG);
	ret |= (0x1 << 0);
	__raw_writel(ret, DSP_AUDIO_CONFIG_REG);

	/* Release the AXI reset */
	ret = __raw_readl(DSP_AUDIO_CONFIG_REG);
	ret |= (0x1 << 1);
	__raw_writel(ret, DSP_AUDIO_CONFIG_REG);
	udelay(100);

	/* devices */
	udelay(100);
	__raw_writel(0x8, DSA_SSP_CLK_RES_CTRL);
	__raw_writel(0x9, DSA_SSP_CLK_RES_CTRL);
	udelay(100);
	__raw_writel(0x8, DSA_ABU_CLK_RES_CTRL);
	__raw_writel(0x9, DSA_ABU_CLK_RES_CTRL);
	udelay(100);

	ret = __raw_readl(MPMU_CCGR);
	ret |= (0x1 << 5);
	__raw_writel(ret, MPMU_CCGR);

	udelay(100);
}

/*
 * currently we don't have chance to use this function, but we
 * would keep this function here for future power optimization,
 * such as low power mode(suspend/resume). the power-off sequence
 * is set according to spec.
 */
#if 0
static void audio_subsystem_poweroff(void)
{
	unsigned int ret;
	/* enable isolation */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret &= ~(0x1 << 3);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);
	udelay(100);
	/* assert AXI and peripheral reset */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret &= ~(0x3);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);
	udelay(100);
	/* gate axi and peripheral clock */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret &= ~(0x18);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);
	udelay(100);
	/* power off */
	ret = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	ret &= ~(0x600);
	__raw_writel(ret, APMU_AUDIO_CLK_RES_CTRL);
	udelay(100);
}
#endif

static int codec_elba_init(struct snd_soc_pcm_runtime *rtd)
{
	audio_subsystem_poweron();
	return 0;
}

/* machine stream operations */
static struct snd_soc_ops mmp3asoc_machine_ops[] = {
	{
	 .startup = mmp3asoc_elba_startup,
	 .hw_params = mmp3asoc_elba_hw_params,
	 },
};

/* digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link mmp3_asoc_elba_dai[] = {
	{
	 .name = "ELBA I2S",
	 .stream_name = "I2S Audio",
	 .codec_name = "88pm80x-codec",
	 .platform_name = "mmp-pcm-audio",
	 .cpu_dai_name = "mmp-sspa-dai.0",
	 .codec_dai_name = "88pm805-i2s",
	 .ops = &mmp3asoc_machine_ops[0],
	 .init = codec_elba_init,
	 },
};

/* audio machine driver */
static struct snd_soc_card mmp_audio_card = {
	 .name = "mmp audio",
	 .dai_link = mmp3_asoc_elba_dai,
	 .num_links = 1,
#ifdef CONFIG_PM_SLEEP
	 .suspend_post = mmp3asoc_suspend_post,
	 .resume_pre = mmp3asoc_resume_pre,
#endif
};

static int __devinit mmp_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	int ret;

	audio_clk = clk_get(NULL, "mmp-audio");
	if (IS_ERR(audio_clk))
		return PTR_ERR(audio_clk);

	card = &mmp_audio_card;
	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "%s: failed: %d\n", __func__, ret);

	return ret;
}

static int __devexit mmp_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card;

	clk_put(audio_clk);
	card = &mmp_audio_card;
	snd_soc_unregister_card(card);

	return 0;
}


static struct platform_driver mmp_audio_driver = {
	.driver		= {
		.name	= "mmp-audio",
		.owner	= THIS_MODULE,
	},
	.probe		= mmp_audio_probe,
	.remove		= __devexit_p(mmp_audio_remove),
};

module_platform_driver(mmp_audio_driver);

/* Module information */
MODULE_AUTHOR("Qiao Zhou, <zhouqiao@marvell.com>");
MODULE_DESCRIPTION("ALSA SoC MMP3");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mmp-audio");
