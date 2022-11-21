/*
 * linux/sound/soc/pxa/emei-dkb.c
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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <sound/pcm_params.h>
#include <linux/pxa2xx_ssp.h>
#ifdef CONFIG_MACH_LT02
#include <mach/mfp-pxa986-lt02.h>
#else
#include <mach/mfp-pxa988-aruba.h>
#endif

#include "pxa-ssp.h"
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
static int ssp_master;
static int gssp_master;
/*
 * SSP audio private data
 */
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
#ifdef CONFIG_PROC_FS
static ssize_t ssp_master_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char a;
	if (copy_from_user(&a, buff, 1))
		return -EINVAL;
	switch (a) {
	case '0':
		pr_info("Switch to SSP slave, codec master mode\n");
		ssp_master = 0;
		break;
	case '1':
		pr_info("Switch to SSP master, codec slave mode\n");
		ssp_master = 1;
		break;
	default:
		pr_err("Wrong input, please echo 1 for ssp master, 0 for codec master\n");
		break;
	}
	return len;
}
static void create_ssp_master_proc_file(void)
{
	struct proc_dir_entry *proc_file = NULL;
	proc_file = create_proc_entry("driver/ssp_master", 0644, NULL);
	if (!proc_file) {
		pr_err("%s: create proc file failed\n", __func__);
		return;
	}
	proc_file->write_proc = (write_proc_t *)ssp_master_write_proc;
}

static ssize_t gssp_master_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char a;

	if (copy_from_user(&a, buff, 1))
		return -EINVAL;
	switch (a) {
		case '0':
			pr_info("Switch to GSSP slave, codec master mode\n");
			gssp_master = 0;
			break;
		case '1':
			pr_info("Switch to GSSP master, codec slave mode\n");
			gssp_master = 1;
			break;
		default:
			pr_err("Wrong input, please echo 1 for gssp master, 0 for codec master\n");
			break;
	}
	
	return len;
}

static void create_gssp_master_proc_file(void)
{
	struct proc_dir_entry *proc_file = NULL;

	proc_file = create_proc_entry("driver/gssp_master", 0644, NULL);
	if (!proc_file) {
		pr_err("%s: create proc file failed\n", __func__);
		return;
	}

	proc_file->write_proc = (write_proc_t *)gssp_master_write_proc;
}
#endif

static void emei_dkb_mfp_init(void)
{
	static bool mfp_init;
	unsigned long emei_dkb_i2s_cfg[] = {
		GPIO021_I2S_BITCLK,
		GPIO022_I2S_SYNC,
		GPIO023_I2S_DATA_OUT,
		GPIO024_I2S_SDATA_IN
	};

	if (!mfp_init) {
		mfp_init = true;
		mfp_config(emei_dkb_i2s_cfg, ARRAY_SIZE(emei_dkb_i2s_cfg));
	}

	return;
}

static int emei_dkb_hifi_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	/* init i2s pins for hifi, only for once */
	emei_dkb_mfp_init();

	cpu_dai->driver->playback.rates = SNDRV_PCM_RATE_44100;
	cpu_dai->driver->capture.rates = SNDRV_PCM_RATE_44100;

	return 0;
}

static int emei_dkb_hifi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	if (ssp_master)
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	else
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);

	return 0;
}

static int emei_dkb_hifi_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct ssp_priv *priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct ssp_device *ssp = priv->ssp;
	u32 sscr0, sscr1;

	sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
	sscr1 = __raw_readl(ssp->mmio_base + SSCR1);

	if (ssp_master)
		sscr1 = (sscr1 & (~0x03000000)) | 0x00b01DC0;
	else
		sscr1 |= 0x03b01DC0;

	__raw_writel(sscr0 | 0x41D0003F, ssp->mmio_base + SSCR0);
	__raw_writel(sscr1, ssp->mmio_base + SSCR1);
	__raw_writel(0x02100004, ssp->mmio_base + SSPSP);

	return 0;
}

static int emei_dkb_lofi_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	/* support narrow band: 8K */
	cpu_dai->driver->playback.rates = SNDRV_PCM_RATE_8000;
	cpu_dai->driver->capture.rates = SNDRV_PCM_RATE_8000;
	codec_dai->driver->playback.channels_max = 1;
	codec_dai->driver->capture.channels_max = 1;

	return 0;
}

static int emei_dkb_dummy_nb_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	/* support narrow band: 8K */
	cpu_dai->driver->playback.rates = SNDRV_PCM_RATE_8000;
	cpu_dai->driver->capture.rates = SNDRV_PCM_RATE_8000;
	codec_dai->driver->playback.channels_max = 1;
	codec_dai->driver->capture.channels_max = 1;
	codec_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S32_LE;
	codec_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S32_LE;

	return 0;
}

static int emei_dkb_dummy_wb_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	/* support wide band: 16K */
	cpu_dai->driver->playback.rates = SNDRV_PCM_RATE_16000;
	cpu_dai->driver->capture.rates = SNDRV_PCM_RATE_16000;
	codec_dai->driver->playback.channels_max = 1;
	codec_dai->driver->capture.channels_max = 1;
	codec_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S32_LE;
	codec_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S32_LE;

	return 0;
}

static int emei_dkb_lofi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct ssp_priv *priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct ssp_device *ssp = priv->ssp;
	u32 sscr0;

	sscr0 = __raw_readl(ssp->mmio_base + SSCR0);

	/*
	 * gssp(ssp-dai.4) is shared by AP and CP. AP would read the gssp
	 * register configured by CP after voice call. it would impact DMA
	 * configuration. AP and CP use different GSSP configuration, and
	 * CP's configuration would set DMA to 16bit width while AP set it
	 * to 32 bit. so we need to re-init GSSP register setting.
	 */
	if (!(sscr0 & SSCR0_SSE)) {
		__raw_writel(0x0, ssp->mmio_base + SSCR0);
		__raw_writel(0x0, ssp->mmio_base + SSCR1);
		__raw_writel(0x0, ssp->mmio_base + SSPSP);
		__raw_writel(0x0, ssp->mmio_base + SSTSA);
	}

#if 0
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
#endif

	return 0;
}

static int emei_dkb_lofi_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct ssp_priv *priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct ssp_device *ssp = priv->ssp;
	u32 sscr0, sscr1;

	sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
	sscr1 = __raw_readl(ssp->mmio_base + SSCR1);

	if (gssp_master)
		sscr1 = (sscr1 & (~0x03000000)) | 0x60b01C01;
	else
		sscr1 = sscr1 | 0x63301C01;

	__raw_writel(sscr0 | 0xC0C0001F, ssp->mmio_base + SSCR0);
	__raw_writel(sscr1, ssp->mmio_base + SSCR1);
	__raw_writel(0x1, ssp->mmio_base + SSPSP);
	__raw_writel(0x1, ssp->mmio_base + SSTSA);
	__raw_writel(0x1, ssp->mmio_base + SSRSA);

	return 0;
}

static struct snd_soc_ops emei_dkb_machine_ops[] = {
{
	.startup = emei_dkb_hifi_startup,
	.hw_params = emei_dkb_hifi_hw_params,
	.prepare = emei_dkb_hifi_prepare,
},
{
	.startup = emei_dkb_lofi_startup,
	.hw_params = emei_dkb_lofi_hw_params,
	.prepare = emei_dkb_lofi_prepare,
},
{
	.startup = emei_dkb_dummy_nb_startup,
	.hw_params = emei_dkb_lofi_hw_params,
	.prepare = emei_dkb_lofi_prepare,
},
{
	.startup = emei_dkb_dummy_wb_startup,
	.hw_params = emei_dkb_lofi_hw_params,
	.prepare = emei_dkb_lofi_prepare,
},
};

static struct snd_soc_dai_link emei_dkb_hifi_dai[] = {
{
	 .name = "88pm805 i2s",
	 .stream_name = "audio playback",
	 .codec_name = "88pm80x-codec",
	 .platform_name = "mmp-pcm-audio",
	 .cpu_dai_name = "pxa-ssp-dai.1",
	 .codec_dai_name = "88pm805-i2s",
	 .ops = &emei_dkb_machine_ops[0],
},
{
	 .name = "88pm805 pcm",
	 .stream_name = "audio capture",
	 .codec_name = "88pm80x-codec",
	 .platform_name = "pxa-pcm-audio",
	 .cpu_dai_name = "pxa-ssp-dai.4",
	 .codec_dai_name = "88pm805-pcm",
	 .ops = &emei_dkb_machine_ops[1],
},
{
	 .name = "88pm805 dummy nb",
	 .stream_name = "dummy nb capture",
	 .codec_name = "88pm80x-codec",
	 .platform_name = "pxa-pcm-audio",
	 .cpu_dai_name = "pxa-ssp-dai.4",
	 .codec_dai_name = "88pm805-dummy",
	 .ops = &emei_dkb_machine_ops[2],
},
{
	 .name = "88pm805 dummy wb",
	 .stream_name = "dummy wb capture",
	 .codec_name = "88pm80x-codec",
	 .platform_name = "pxa-pcm-audio",
	 .cpu_dai_name = "pxa-ssp-dai.4",
	 .codec_dai_name = "88pm805-dummy",
	 .ops = &emei_dkb_machine_ops[3],
},
};

static struct snd_soc_card emei_dkb_card = {
	.name = "emei-dkb-hifi",
	.dai_link = emei_dkb_hifi_dai,
	.num_links = ARRAY_SIZE(emei_dkb_hifi_dai),
};

#ifdef CONFIG_PM
static int emei_dkb_suspend(struct device *dev)
{
	snd_soc_suspend(dev);
	return 0;
}

static int emei_dkb_resume(struct device *dev)
{
	snd_soc_resume(dev);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(emei_dkb_pm_ops, emei_dkb_suspend,
			 emei_dkb_resume);

static int __devinit emei_dkb_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &emei_dkb_card;
	int ret;

	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n",
			ret);
		return ret;
	}
#ifdef CONFIG_PROC_FS
	create_ssp_master_proc_file();
	create_gssp_master_proc_file();
#endif

	return ret;
}

static int __devexit emei_dkb_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver emei_dkb_driver = {
	.driver		= {
		.name	= "emei-dkb-hifi",
		.owner	= THIS_MODULE,
		.pm		= &emei_dkb_pm_ops,
	},
	.probe		= emei_dkb_probe,
	.remove		= __devexit_p(emei_dkb_remove),
};

module_platform_driver(emei_dkb_driver);

/* Module information */
MODULE_AUTHOR("zhouqiao@marvell.com");
MODULE_DESCRIPTION("ALSA SoC TTC DKB");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:emei-dkb-hifi");
