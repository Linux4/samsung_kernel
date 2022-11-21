/*
 * linux/sound/soc/pxa/ttc_dkb.c
 *
 * Copyright (C) 2007 Marvell International Ltd.
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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/pxa2xx_ssp.h>
#include <sound/pcm_params.h>
#include "../codecs/88pm860x-codec.h"

/* SSP audio private data */
struct ssp_priv {
	struct ssp_device *ssp;
	unsigned int sysclk;
	int dai_fmt;
#ifdef CONFIG_PM
	uint32_t	cr0;
	uint32_t	cr1;
	uint32_t	to;
	uint32_t	psp;
#endif
};

static int ttc_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct ssp_priv *priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct ssp_device *ssp = priv->ssp;
	u32 sscr0;

	sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
	/*
	 * gssp(ssp-dai.4) is shared by AP and CP. AP would read the gssp register
	 * configured by CP after voice call. it would impact DMA configuration. AP
	 * and CP use different GSSP configuration, and CP's configuration would set
	 * DMA to 16bit width while AP set it to 32 bit. so we need to re-init GSSP
	 * register setting.
	 */
	if (!memcmp(cpu_dai->name, "pxa-ssp-dai.4", sizeof("pxa-ssp-dai.4")) && !(sscr0 & SSCR0_SSE)) {
		__raw_writel(0x0, ssp->mmio_base + SSCR0);
		__raw_writel(0x0, ssp->mmio_base + SSCR1);
		__raw_writel(0x0, ssp->mmio_base + SSPSP);
		__raw_writel(0x0, ssp->mmio_base + SSTSA);
	}

	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);

	return 0;
}

static struct snd_soc_ops ttc_pm860x_machine_ops = {
	.hw_params = ttc_hw_params,
};

static int ttc_pm860x_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* do not use DAPM, set all pin to NC */
	/* input widget */
	snd_soc_dapm_nc_pin(dapm, "AUX1");
	snd_soc_dapm_nc_pin(dapm, "AUX2");
	snd_soc_dapm_nc_pin(dapm, "MIC1P");
	snd_soc_dapm_nc_pin(dapm, "MIC1N");
	snd_soc_dapm_nc_pin(dapm, "MIC2P");
	snd_soc_dapm_nc_pin(dapm, "MIC2N");
	snd_soc_dapm_nc_pin(dapm, "MIC3P");
	snd_soc_dapm_nc_pin(dapm, "MIC3N");

	/* output widget */
	snd_soc_dapm_nc_pin(dapm, "HS1");
	snd_soc_dapm_nc_pin(dapm, "HS2");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2");
	snd_soc_dapm_nc_pin(dapm, "EARP");
	snd_soc_dapm_nc_pin(dapm, "EARN");
	snd_soc_dapm_nc_pin(dapm, "LSP");
	snd_soc_dapm_nc_pin(dapm, "LSN");

	return snd_soc_dapm_sync(dapm);
}

/* ttc/td-dkb digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link ttc_pm860x_hifi_dai[] = {
{
	 .name = "88pm860x i2s",
	 .stream_name = "audio playback",
	 .codec_name = "88pm860x-codec",
	 .platform_name = "mmp-pcm-audio",
	 .cpu_dai_name = "pxa-ssp-dai.1",
	 .codec_dai_name = "88pm860x-i2s",
	 .init = ttc_pm860x_init,
	 .ops = &ttc_pm860x_machine_ops,
},
{
	 .name = "88pm860x pcm",
	 .stream_name = "audio capture",
	 .codec_name = "88pm860x-codec",
	 .platform_name = "pxa-pcm-audio",
	 .cpu_dai_name = "pxa-ssp-dai.4",
	 .codec_dai_name = "88pm860x-pcm",
	 .init = ttc_pm860x_init,
	 .ops = &ttc_pm860x_machine_ops,
},
};

/* ttc/td audio machine driver */
static struct snd_soc_card ttc_dkb_card = {
	.name = "ttc-dkb-hifi",
	.dai_link = ttc_pm860x_hifi_dai,
	.num_links = ARRAY_SIZE(ttc_pm860x_hifi_dai),
};

static int __devinit ttc_dkb_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &ttc_dkb_card;
	int ret;

	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n",
			ret);

	return ret;
}

static int __devexit ttc_dkb_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver ttc_dkb_driver = {
	.driver		= {
		.name	= "ttc-dkb-audio",
		.owner	= THIS_MODULE,
	},
	.probe		= ttc_dkb_probe,
	.remove		= __devexit_p(ttc_dkb_remove),
};

module_platform_driver(ttc_dkb_driver);

/* Module information */
MODULE_AUTHOR("Qiao Zhou, <zhouqiao@marvell.com>");
MODULE_DESCRIPTION("ALSA SoC TTC DKB");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ttc-dkb-audio");
