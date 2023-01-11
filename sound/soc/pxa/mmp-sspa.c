/*
 * linux/sound/soc/pxa/mmp-sspa.c
 * Base on pxa2xx-ssp.c
 *
 * Copyright (C) 2011 Marvell International Ltd.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pxa2xx_ssp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/dmaengine.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/pxa2xx-lib.h>
#include <sound/dmaengine_pcm.h>
#include "mmp-sspa.h"

/*
 * SSPA audio private data
 */
struct sspa_priv {
	struct ssp_device *sspa;
	struct snd_dmaengine_dai_dma_data *dma_params;
	int running_cnt;
	int txsp;
	int rxsp;
	int txctl;
	int rxctl;
	int txfifo;
	int rxfifo;
	unsigned int burst_size;
};

static void mmp_sspa_write_reg(struct ssp_device *sspa, u32 reg, u32 val)
{
	__raw_writel(val, sspa->mmio_base + reg);
}

static u32 mmp_sspa_read_reg(struct ssp_device *sspa, u32 reg)
{
	return __raw_readl(sspa->mmio_base + reg);
}

static void mmp_sspa_tx_enable(struct ssp_device *sspa)
{
	unsigned int sspa_sp;

	sspa_sp = mmp_sspa_read_reg(sspa, SSPA_TXSP);
	sspa_sp &= ~SSPA_SP_S_RST;
	sspa_sp |= SSPA_SP_S_EN;
	mmp_sspa_write_reg(sspa, SSPA_TXSP, sspa_sp);
}

static void mmp_sspa_tx_disable(struct ssp_device *sspa)
{
	unsigned int sspa_sp;

	sspa_sp = mmp_sspa_read_reg(sspa, SSPA_TXSP);
	sspa_sp &= ~SSPA_SP_S_EN;
	sspa_sp |= SSPA_SP_S_RST;
	sspa_sp |= SSPA_SP_FFLUSH;
	mmp_sspa_write_reg(sspa, SSPA_TXSP, sspa_sp);
}

static void mmp_sspa_rx_enable(struct ssp_device *sspa)
{
	unsigned int sspa_sp;

	sspa_sp = mmp_sspa_read_reg(sspa, SSPA_RXSP);
	sspa_sp &= ~SSPA_SP_S_RST;
	sspa_sp |= SSPA_SP_S_EN;
	mmp_sspa_write_reg(sspa, SSPA_RXSP, sspa_sp);
	udelay(1);
}

static void mmp_sspa_rx_disable(struct ssp_device *sspa)
{
	unsigned int sspa_sp;

	sspa_sp = mmp_sspa_read_reg(sspa, SSPA_RXSP);
	sspa_sp &= ~SSPA_SP_S_EN;
	sspa_sp |= SSPA_SP_S_RST;
	sspa_sp |= SSPA_SP_FFLUSH;

	mmp_sspa_write_reg(sspa, SSPA_RXSP, sspa_sp);
}

static int mmp_sspa_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct sspa_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (dai->active)
		return 0;

	if (priv->sspa->clk)
		clk_prepare_enable(priv->sspa->clk);

	return 0;
}

static void mmp_sspa_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct sspa_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (dai->active)
		return;

	if (priv->sspa->clk)
		clk_disable_unprepare(priv->sspa->clk);

	return;
}

/*
 * Set the SSP ports SYSCLK.
 */
static int mmp_sspa_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
				    int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int mmp_sspa_set_dai_pll(struct snd_soc_dai *cpu_dai, int pll_id,
				 int source, unsigned int freq_in,
				 unsigned int freq_out)
{
	return 0;
}

/*
 * Set up the sspa dai format. The sspa port must be inactive
 * before calling this function as the physical
 * interface format is changed.
 */
static int mmp_sspa_set_dai_fmt(struct snd_soc_dai *cpu_dai,
				 unsigned int fmt)
{
	struct sspa_priv *sspa_priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct ssp_device *sspa = sspa_priv->sspa;
	u32 sspa_sp, sspa_ctrl, sp_reg, ctrl_reg;

	if (cpu_dai->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sp_reg = SSPA_TXSP;
		ctrl_reg = SSPA_TXCTL;
	} else if (cpu_dai->stream == SNDRV_PCM_STREAM_CAPTURE) {
		sp_reg = SSPA_RXSP;
		ctrl_reg = SSPA_RXCTL;
	} else {
		/* don't configure anything */
		return 0;
	}
	/* reset port settings */
	sspa_sp = SSPA_SP_WEN | SSPA_SP_FIX;
	sspa_ctrl = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		sspa_sp |= SSPA_SP_MSL;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		break;
	default:
		return -EINVAL;
	}

	sspa_sp &= ~SSPA_SP_FSP;
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		sspa_sp |= SSPA_SP_FSP;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		sspa_sp |= SSPA_TXSP_FPER(63);
		sspa_sp |= SSPA_SP_FWID(31);
		sspa_ctrl |= SSPA_CTL_XDATDLY(1);
		sspa_ctrl &= ~SSPA_CTL_XWDLEN1_MASK;
		sspa_ctrl |= SSPA_CTL_XWDLEN1(SSPA_CTL_32_BITS);
		/* fix to sample shift issue in I2S mode */
		if (cpu_dai->stream == SNDRV_PCM_STREAM_PLAYBACK)
			sspa_sp |= SSPA_SP_FSP;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		sspa_sp |= SSPA_TXSP_FPER(31);
		sspa_sp |= SSPA_SP_FWID(0);
		sspa_ctrl |= SSPA_CTL_XDATDLY(1);
		sspa_ctrl &= ~SSPA_CTL_XWDLEN1_MASK;
		sspa_ctrl |= SSPA_CTL_XWDLEN1(SSPA_CTL_16_BITS);
		break;
	case SND_SOC_DAIFMT_DSP_B:
		sspa_sp |= SSPA_TXSP_FPER(31);
		sspa_sp |= SSPA_SP_FWID(0);
		sspa_ctrl |= SSPA_CTL_XDATDLY(0);
		sspa_ctrl &= ~SSPA_CTL_XWDLEN1_MASK;
		sspa_ctrl |= SSPA_CTL_XWDLEN1(SSPA_CTL_16_BITS);
		break;
	default:
		return -EINVAL;
	}

	mmp_sspa_write_reg(sspa, sp_reg, sspa_sp);
	mmp_sspa_write_reg(sspa, ctrl_reg, sspa_ctrl);

	return 0;
}

/*
 * Set the SSPA audio DMA parameters and sample size.
 * Can be called multiple times by oss emulation.
 */
static int mmp_sspa_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sspa_priv *sspa_priv = snd_soc_dai_get_drvdata(dai);
	struct ssp_device *sspa = sspa_priv->sspa;
	struct snd_dmaengine_dai_dma_data *dma_params;
	u32 sspa_ctrl;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		sspa_ctrl = mmp_sspa_read_reg(sspa, SSPA_TXCTL);
	else
		sspa_ctrl = mmp_sspa_read_reg(sspa, SSPA_RXCTL);

	sspa_ctrl &= ~SSPA_CTL_XFRLEN1_MASK;
	sspa_ctrl |= SSPA_CTL_XFRLEN1(params_channels(params) - 1);
	sspa_ctrl &= ~SSPA_CTL_XSSZ1_MASK;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		sspa_ctrl |= SSPA_CTL_XSSZ1(SSPA_CTL_8_BITS);
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		sspa_ctrl |= SSPA_CTL_XSSZ1(SSPA_CTL_16_BITS);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		sspa_ctrl |= SSPA_CTL_XSSZ1(SSPA_CTL_20_BITS);
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		sspa_ctrl |= SSPA_CTL_XSSZ1(SSPA_CTL_24_BITS);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sspa_ctrl |= SSPA_CTL_XSSZ1(SSPA_CTL_32_BITS);
		break;
	default:
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mmp_sspa_write_reg(sspa, SSPA_TXCTL, sspa_ctrl);
		mmp_sspa_write_reg(sspa, SSPA_TXFIFO_LL, 0x1);
	} else {
		mmp_sspa_write_reg(sspa, SSPA_RXCTL, sspa_ctrl);
		mmp_sspa_write_reg(sspa, SSPA_RXFIFO_UL, 0x0);
	}

	dma_params = &sspa_priv->dma_params[substream->stream];
	dma_params->addr = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
				(sspa->phys_base + SSPA_TXD) :
				(sspa->phys_base + SSPA_RXD);

	dma_params->maxburst = sspa_priv->burst_size;
	snd_soc_dai_set_dma_data(cpu_dai, substream, dma_params);
	return 0;
}

static int mmp_sspa_trigger(struct snd_pcm_substream *substream, int cmd,
			     struct snd_soc_dai *dai)
{
	struct sspa_priv *sspa_priv = snd_soc_dai_get_drvdata(dai);
	struct ssp_device *sspa = sspa_priv->sspa;
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			mmp_sspa_tx_enable(sspa);
		else
			mmp_sspa_rx_enable(sspa);

		sspa_priv->running_cnt++;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sspa_priv->running_cnt--;

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			mmp_sspa_tx_disable(sspa);
		else
			mmp_sspa_rx_disable(sspa);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int mmp_sspa_probe(struct snd_soc_dai *dai)
{
	struct sspa_priv *priv = dev_get_drvdata(dai->dev);

	snd_soc_dai_set_drvdata(dai, priv);
	return 0;

}

#define MMP_SSPA_RATES SNDRV_PCM_RATE_8000_192000
#define MMP_SSPA_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
		SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | \
		SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops mmp_sspa_dai_ops = {
	.startup	= mmp_sspa_startup,
	.shutdown	= mmp_sspa_shutdown,
	.trigger	= mmp_sspa_trigger,
	.hw_params	= mmp_sspa_hw_params,
	.set_sysclk	= mmp_sspa_set_dai_sysclk,
	.set_pll	= mmp_sspa_set_dai_pll,
	.set_fmt	= mmp_sspa_set_dai_fmt,
};

#ifdef CONFIG_PM
static int mmp_sspa_suspend(struct snd_soc_dai *cpu_dai)
{
	struct sspa_priv *priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct ssp_device *sspa = priv->sspa;

	priv->txsp = __raw_readl(sspa->mmio_base + SSPA_TXSP);
	priv->rxsp = __raw_readl(sspa->mmio_base + SSPA_RXSP);
	priv->txctl = __raw_readl(sspa->mmio_base + SSPA_TXCTL);
	priv->rxctl = __raw_readl(sspa->mmio_base + SSPA_RXCTL);
	priv->txfifo = __raw_readl(sspa->mmio_base + SSPA_TXFIFO_LL);
	priv->rxfifo = __raw_readl(sspa->mmio_base + SSPA_RXFIFO_UL);

	return 0;
}

static int mmp_sspa_resume(struct snd_soc_dai *cpu_dai)
{
	struct sspa_priv *priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct ssp_device *sspa = priv->sspa;

	priv->txsp |= SSPA_SP_WEN;
	__raw_writel(priv->txsp, sspa->mmio_base + SSPA_TXSP);
	priv->rxsp |= SSPA_SP_WEN;
	__raw_writel(priv->rxsp, sspa->mmio_base + SSPA_RXSP);
	__raw_writel(priv->txctl, sspa->mmio_base + SSPA_TXCTL);
	__raw_writel(priv->rxctl, sspa->mmio_base + SSPA_RXCTL);
	__raw_writel(priv->txfifo, sspa->mmio_base + SSPA_TXFIFO_LL);
	__raw_writel(priv->rxfifo, sspa->mmio_base + SSPA_RXFIFO_UL);

	return 0;
}
#else
#define mmp_sspa_suspend	NULL
#define mmp_sspa_resume		NULL
#endif

static struct snd_soc_dai_driver mmp_sspa_dai = {
	.probe = mmp_sspa_probe,
	.suspend = mmp_sspa_suspend,
	.resume = mmp_sspa_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 128,
		.rates = MMP_SSPA_RATES,
		.formats = MMP_SSPA_FORMATS,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = MMP_SSPA_RATES,
		.formats = MMP_SSPA_FORMATS,
	},
	.ops = &mmp_sspa_dai_ops,
};

static const struct snd_soc_component_driver mmp_sspa_component = {
	.name		= "mmp-sspa",
};

#ifdef CONFIG_OF
static const struct of_device_id pxa_ssp_of_ids[] = {
	{ .compatible = "mrvl,mmp-sspa-dai", },
};
#endif


static int asoc_mmp_sspa_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sspa_priv *priv;
	struct resource *res;
	char const *platform_driver_name;
	int ret;

	priv = devm_kzalloc(&pdev->dev,
				sizeof(struct sspa_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->sspa = devm_kzalloc(&pdev->dev,
				sizeof(struct ssp_device), GFP_KERNEL);
	if (priv->sspa == NULL)
		return -ENOMEM;

	priv->sspa->pdev = pdev;
	priv->dma_params = devm_kzalloc(&pdev->dev,
			2 * sizeof(struct snd_dmaengine_dai_dma_data),
			GFP_KERNEL);
	if (priv->dma_params == NULL)
		return -ENOMEM;


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no memory resource defined\n");
		return -ENODEV;
	}

	priv->sspa->mmio_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->sspa->mmio_base))
		return PTR_ERR(priv->sspa->mmio_base);

	priv->sspa->phys_base = res->start;

	if (of_property_read_string(np,
				"platform_driver_name",
				&platform_driver_name)) {
		dev_err(&pdev->dev,
			"Missing platform_driver_name property in the DT\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "burst_size",
				     &priv->burst_size)) {
		dev_err(&pdev->dev,
			"Missing DMA burst size\n");
		return -EINVAL;
	}


	priv->sspa->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->sspa->clk))
		return PTR_ERR(priv->sspa->clk);

	platform_set_drvdata(pdev, priv);

	ret = devm_snd_soc_register_component(&pdev->dev, &mmp_sspa_component,
					       &mmp_sspa_dai, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to register DAI\n");
		return ret;
	}

	if (strcmp(platform_driver_name, "tdma_platform") == 0)
		ret = mmp_pcm_platform_register(&pdev->dev);

	return ret;
}

static int asoc_mmp_sspa_remove(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	char const *platform_driver_name;

	if (of_property_read_string(np,
				"platform_driver_name",
				&platform_driver_name)) {
		dev_err(&pdev->dev,
			"Missing platform_driver_name property in the DT\n");
		return -EINVAL;
	}

	if (strcmp(platform_driver_name, "tdma_platform") == 0)
		mmp_pcm_platform_unregister(&pdev->dev);

	return 0;
}

static struct platform_driver asoc_mmp_sspa_driver = {
	.driver = {
		.name = "mmp-sspa-dai",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pxa_ssp_of_ids),
	},
	.probe = asoc_mmp_sspa_probe,
	.remove = asoc_mmp_sspa_remove,
};

module_platform_driver(asoc_mmp_sspa_driver);

MODULE_AUTHOR("Leo Yan <leoy@marvell.com>");
MODULE_DESCRIPTION("MMP SSPA SoC Interface");
MODULE_LICENSE("GPL");
