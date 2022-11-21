/*
 * linux/sound/soc/pxa/mmp3-sspa-zsp.c
 * Base on mmp-ssp-zsp.c
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <linux/io.h>
#include <mach/regs-sspa.h>
#include <mach/pxa910-squ.h>
#include <mach/mmp-zmq.h>
#include <plat/dma.h>
#include <plat/ssp.h>

#include "mmp-zsp-audio.h"
#include "mmp3-sspa-zsp.h"
static int mmp_zsp_sspa_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static void mmp_zsp_sspa_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return;
}

#ifdef CONFIG_PM
static int mmp_zsp_sspa_suspend(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int mmp_zsp_sspa_resume(struct snd_soc_dai *cpu_dai)
{
	return 0;
}
#else
#define mmp_zsp_sspa_suspend	NULL
#define mmp_zsp_sspa_resume	NULL
#endif

/*
 * Set the SSP ports SYSCLK.
 */
static int mmp_zsp_sspa_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct zsp_sspa *zsp_sspa = snd_soc_dai_get_drvdata(cpu_dai);
	zsp_sspa->sspa_pll_clk = freq;
	return 0;
}

static int mmp_zsp_sspa_set_dai_pll(struct snd_soc_dai *cpu_dai, int pll_id,
				 int source, unsigned int freq_in,
				 unsigned int freq_out)
{
	struct zsp_sspa *zsp_sspa = snd_soc_dai_get_drvdata(cpu_dai);

	switch (pll_id) {
	case MMP_SYSCLK:
		zsp_sspa->zsp_sspa_conf.mclk = freq_out;
		break;
	default:
		return -ENODEV;
	}
	return 0;
}


/*
 * Set the SSPA audio DMA parameters and sample size.
 * Can be called multiple times by oss emulation.
 */
static int mmp_zsp_sspa_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct zsp_sspa *zsp_sspa = snd_soc_dai_get_drvdata(dai);
	u32 sspactl;

	/* bit size */
	sspactl = 0;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		sspactl &= ~0x7;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		sspactl &= ~0x7;
		sspactl |= 0x2;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		sspactl &= ~0x7;
		sspactl |= 0x3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		sspactl &= ~0x7;
		sspactl |= 0x4;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sspactl &= ~0x7;
		sspactl |= 0x5;
		break;
	default:
		return -EINVAL;
	}

	zsp_sspa->zsp_sspa_conf.ch_num = params_channels(params);
	zsp_sspa->zsp_sspa_conf.sample_size = sspactl;
	zsp_sspa->zsp_sspa_conf.sample_rate = params_rate(params);

	return 0;
}
/*
 * Set up the sspa dai format. The sspa port must be inactive
 * before calling this function as the physical
 * interface format is changed.
 */
static int mmp_zsp_sspa_set_dai_fmt(struct snd_soc_dai *cpu_dai,
				 unsigned int fmt)
{
	struct zsp_sspa *zsp_sspa = snd_soc_dai_get_drvdata(cpu_dai);
	pr_debug("%s: enter\n", __func__);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		zsp_sspa->zsp_sspa_conf.msl_select = SSPA_CONFIG_MS_SEL_MASTER;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		zsp_sspa->zsp_sspa_conf.msl_select = SSPA_CONFIG_MS_SEL_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		zsp_sspa->zsp_sspa_conf.fsp = SSPA_CONFIG_FRAME_SYNC_POLA_LOW;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		zsp_sspa->zsp_sspa_conf.fsp = SSPA_CONFIG_FRAME_SYNC_POLA_HIGH;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		zsp_sspa->zsp_sspa_conf.word_length =
				SSPA_CONFIG_WORD_LENGTH_32_BITS;
		zsp_sspa->zsp_sspa_conf.fwid = 32;
		zsp_sspa->zsp_sspa_conf.fsync_active = 64;
		zsp_sspa->zsp_sspa_conf.jst = SSPA_CONFIG_SAMPLE_JST_LEFT;
		zsp_sspa->zsp_sspa_conf.data_delay =
					SSPA_CONFIG_DATA_DELAY_1_BIT;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int mmp_zsp_sspa_trigger(struct snd_pcm_substream *substream,
			      int cmd, struct snd_soc_dai *dai)
{
	/* nothing to do. everything is done in platform driver */
	return 0;
}

#define MMP_SSPA_RATES 0xffffffff
#define MMP_SSPA_FORMATS 0xffffffff
static int mmp2_sspa_probe(struct snd_soc_dai *dai)
{
	struct zsp_sspa *zsp_sspa;
	int ret;

	/*
	 * for sspa may be linked to different codecs;
	 * such as sspa1 linked to hdmi and codec.
	 */
	zsp_sspa = snd_soc_dai_get_drvdata(dai);
	if (zsp_sspa) {
		printk(KERN_WARNING "%s: this port has been linked\n",
			__func__);
		return 0;
	}

	zsp_sspa = kzalloc(sizeof(struct zsp_sspa), GFP_KERNEL);
	if (!zsp_sspa)
		return -ENOMEM;

	zsp_sspa->sspa = sspa_request(dai->id + 1, "mmp-sspa");

	if (zsp_sspa->sspa == NULL) {
		ret = -ENODEV;
		goto err_priv;
	}
	if (zsp_sspa->sspa->pdev->id == 0) {
		zsp_sspa->sspa_id = MMP_SSPA1;
	} else if (zsp_sspa->sspa->pdev->id == 1) {
		zsp_sspa->sspa_id = MMP_SSPA2;
	}

	snd_soc_dai_set_drvdata(dai, zsp_sspa);
	return 0;

err_priv:
	kfree(zsp_sspa);
	return ret;
}

static int mmp2_sspa_remove(struct snd_soc_dai *dai)
{
	struct zsp_sspa *zsp_sspa = snd_soc_dai_get_drvdata(dai);

	sspa_free(zsp_sspa->sspa);
	kfree(zsp_sspa);
	return 0;
}


static struct snd_soc_dai_ops mmp_zsp_sspa_dai_ops = {
	.startup	= mmp_zsp_sspa_startup,
	.shutdown	= mmp_zsp_sspa_shutdown,
	.trigger	= mmp_zsp_sspa_trigger,
	.set_fmt	= mmp_zsp_sspa_set_dai_fmt,
	.hw_params	= mmp_zsp_sspa_hw_params,
	.set_sysclk	= mmp_zsp_sspa_set_dai_sysclk,
	.set_pll	= mmp_zsp_sspa_set_dai_pll,
};

struct snd_soc_dai_driver mmp_zsp_sspa_dai = {
	.probe = mmp2_sspa_probe,
	.remove = mmp2_sspa_remove,
	.suspend = mmp_zsp_sspa_suspend,
	.resume = mmp_zsp_sspa_resume,
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
	.ops = &mmp_zsp_sspa_dai_ops,
};
EXPORT_SYMBOL_GPL(mmp_zsp_sspa_dai);

static __devinit int asoc_mmp_zsp_sspa_probe(struct platform_device *pdev)
{
	return snd_soc_register_dai(&pdev->dev, &mmp_zsp_sspa_dai);
}

static __devexit int asoc_mmp_zsp_sspa_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver asoc_mmp_zsp_sspa_driver = {
	.driver = {
		.name = "mmp3-sspa-dai",
		.owner = THIS_MODULE,
	},
	.probe = asoc_mmp_zsp_sspa_probe,
	.remove = __devexit_p(asoc_mmp_zsp_sspa_remove),
};

static int __init mmp_zsp_sspa_modinit(void)
{
	return platform_driver_register(&asoc_mmp_zsp_sspa_driver);
}

module_init(mmp_zsp_sspa_modinit);

static void __exit mmp_zsp_sspa_exit(void)
{
	platform_driver_unregister(&asoc_mmp_zsp_sspa_driver);
}
module_exit(mmp_zsp_sspa_exit);

/* Module information */
MODULE_AUTHOR("gwu3@marvell.com");
MODULE_DESCRIPTION("MMP3 ZSP SSPA SoC Interface");
MODULE_LICENSE("GPL");
